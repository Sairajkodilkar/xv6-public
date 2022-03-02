#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
/* Sairaj :
 *	Setup the global descriptor table for the the given cpuid
 */
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
	  /* Sairaj:
	   * if the page directory entry has table allocated to it and it is
	   * present then get the page table address 
	   *
	   * pde --> PDE (20 bit page table virtual address)
	   */
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
	/* Sairaj:
	 * if page table is not allocated and caller asks to allocate it 
	 * then allocat the page table using kalloc
	 */
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
	/* Sairaj:
	 * Store the address of the allocated page table inside the page directory
	 * with the present, writeable and user bits set
	 */
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  /* Sairaj:
   *	Get the address inside the page table where you need to store frame
   *	address
   *
   *	return the pointer to the page table entry for the given virtual
   *	address
   */
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  /* Sairaj:
   *	We need to map that entire page hence round it down
   */
  a = (char*)PGROUNDDOWN((uint)va);
  /* Sairaj:
   *	Get the last page within the given virtual address
   */
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
	  /* Sairaj:
	   *	Allocate the page table entry for the given virtual address
	   */
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
	/* Sairaj: Map physical 0 to 1 MB */
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 /* Sairaj: Map 1 MB to start of data segment*/
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 /* Sairaj: Map start of data segment till the end of the physical memory */
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
/* Sairaj:
 * Here the end is zero because of the unsigned nature of the function using
 * this the -0xFE000000 becomes 0xFFFFFFFF - 0xFE000000 + 1 = size of the remaining space
 * at the top/end of the memory
 */
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
/* Sairaj:
 * This create the nested pagedirectory with 10-10-12 scheme
 * Here each page table is 4K because each entry has 4 bytes and there are 1K
 * such entries
 *
   *	setupkvm does following:
   *	allocate the page directory
   *	maps the kernel address space to that page directory
   *	and return the pagedirectory address
   *	TODO: investiage if the page dir address returned is virtual or
   *	physical
 */
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  /* Sairaj: allocate 4k page */
  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  /* Sairaj: zero the 4k page */
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  /*Sairaj:
   * Why to call this function for each process creation
   *	Map the kernel pages to the pagedirectory of the user process
   *	This is essential as we need to handle the traps which are in kernel
   *	space
   */
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
	  /* Fill up each directory entry corrosponding to top 10 bits of the
	   * virtual address
	   */
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
/* TODO: read about the tasks in the x86
 */
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  /* Sairaj:
   *	This tells that this is the TSS entry for the given process
   *	the segments inside the TSS are used for setting the values of the 
   *	SS and ESP  while changing the PL during the interrupt
   */
  ltr(SEG_TSS << 3); // Sairaj: 5th entry in the LGDTR is the TSS
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
/* Sairaj:
 *	This loads the init inside a page of the memory
 *	steps:
 *		1) get a frame
 *		2) set all zeros to the frame
 *		3) map the frame to a virtual address of the init
 *		4) move all the initcode of size sz into the memory page
 */
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  /* get the free page */
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  /* Map the virtual address 
   * 0 to pagesize to the physical address of the mem
   * This replaces a setupkvm entry which maps the 0 to EXTMEM physical
   * address to the KERNBASE virtual address
   *
   */
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  /* Why this init is just the data
   * TODO: Investigate
   *		initcode exists in a binary format at the end of the kernel code,
   *		so where is this init comming from
   *		Ans this is passed by the userinit
   *
   */
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  /* For each page inside the process's virtual memory get the corrospoinding
   * page table entry pointer
   */
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
	/* Sairaj:
	 * Upper 20 bits i.e. the physical address of the frame allocated to the
	 * process
	 */
    pa = PTE_ADDR(*pte);
	/* Sairaj:
	 *	 check for the last allocation 
	 *	 and assign n which denots the number of bytes to be read from the file
	 */
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
	/* Since we have not yet initialize the CR# with the given virtual memory
	 * of the process we need to use the virtual memory space of the kernel
	 */
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  /* TODO: understand why this is round up 
   *	Because the oldsz is already allocated inside a page 
   */
  a = PGROUNDUP(oldsz);
  /* Sairaj:
   *	Allocate the extra pages from the old size to the new size
   */
  for(; a < newsz; a += PGSIZE){
	  mem = kalloc();
	  if(mem == 0){
		  cprintf("allocuvm out of memory\n");
		  deallocuvm(pgdir, newsz, oldsz);
		  return 0;
	  }
	  memset(mem, 0, PGSIZE);
	  /* Sairaj:
	   * Map the given physical address from the free list to the given virutal
	   * address 
	   */
	  if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
		  cprintf("allocuvm out of memory (2)\n");
		  deallocuvm(pgdir, newsz, oldsz);
		  kfree(mem);
		  return 0;
	  }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
	int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
	pte_t *pte;
	uint a, pa;

	if(newsz >= oldsz)
		return oldsz;

	/* Sairaj:
	 *	This starts deallocating from the next page because part of the current
	 *	page may be in use
	 */
	a = PGROUNDUP(newsz);
	/* Sairaj:
	 *	Deallocate all the memory from the newsize to the oldsize
	 *	page by page
	 */
	for(; a  < oldsz; a += PGSIZE){
		pte = walkpgdir(pgdir, (char*)a, 0);
		if(!pte)
			/* Sairaj:
			 * this simply select the next directory entry 
			 * TODO: investigate Why PGSIZE is minus from it?
			 */
			a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
		else if((*pte & PTE_P) != 0){
			pa = PTE_ADDR(*pte);
			if(pa == 0)
				panic("kfree");
			/* Sairaj:
			 * Get the kernel space virtual address for the given physical
			 * address and add it to the free list
			 */
			char *v = P2V(pa);
			kfree(v);
			*pte = 0;
		}
	}
	return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
	void
freevm(pde_t *pgdir)
{
	uint i;

	if(pgdir == 0)
		panic("freevm: no pgdir");
	/* Sairaj:
	 *	Now this deallocates the memory required by the program but the
	 * allocated page tables are still in use hence free them
	 */
	deallocuvm(pgdir, KERNBASE, 0);
	for(i = 0; i < NPDENTRIES; i++){
		/* Sairaj: free each allocated page */
		if(pgdir[i] & PTE_P){
			char * v = P2V(PTE_ADDR(pgdir[i]));
			kfree(v);
		}
	}
	/* Sairaj: Free the page directory */
	kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
	void
clearpteu(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if(pte == 0)
		panic("clearpteu");
	*pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
/* Sairaj:
 *	steps inside the copyuvm
 *	1) setup the basic page directory for the kernel
 *	2) for each virtual address get the actual frame address from the page
 *	table
 *	3) allocate the new frame
 *	4) copy all the contents of the page from the source frame to newly
 *	allocated frame
 *	5) Go to step 2 until the size of the program is traverse
 *
 *	TODO: 
 *		Investigate if for the programs view virtal addresses starts from the 
 *		0 to size of program
 *		how does the kalloc works?
 */
	pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
	pde_t *d;
	pte_t *pte;
	uint pa, i, flags;
	char *mem;

	if((d = setupkvm()) == 0)
		return 0;
	for(i = 0; i < sz; i += PGSIZE){
		/*Sairaj:
		 * get the page table entry for the virtual address 0 to sz
		 * BUT where is 0 virtual address mapped since all virtual addresses are
		 * wrt to the kernbase
		 */
		if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
			/* Sairaj: Source PTE should always exists since we need to make the
			 * copy for it 
			 */
			panic("copyuvm: pte should exist");
		if(!(*pte & PTE_P))
			/* Sairaj:
			 * if the page table entry has invalid bit set
			 */
			panic("copyuvm: page not present");
		/* Sairaj:
		 *	get the physical address of the frame in the PTE
		 */
		pa = PTE_ADDR(*pte);
		flags = PTE_FLAGS(*pte);
		/* Sairaj:
		 *	allocate the frame 
		 */
		if((mem = kalloc()) == 0)
			goto bad;
		/*  copy the source frame into the newly allocated frame 
		*/
		memmove(mem, (char*)P2V(pa), PGSIZE);
		/* Map the new frame address to the virtual address for the newly allocated
		 * page directory 
		 */
		if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
			kfree(mem);
			goto bad;
		}
	}
	return d;

bad:
	freevm(d);
	return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
	char*
uva2ka(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if((*pte & PTE_P) == 0)
		return 0;
	if((*pte & PTE_U) == 0)
		return 0;
	return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
	int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
	char *buf, *pa0;
	uint n, va0;

	buf = (char*)p;
	while(len > 0){
		va0 = (uint)PGROUNDDOWN(va);
		/* Sairaj:
		 *	This maps the user virtual address to the kernel virtual address
		 *	converts the 20 bit virtual address found inside the PTE
		 *	This adds the 0x8000000 to the user virtual address
		 */
		pa0 = uva2ka(pgdir, (char*)va0);
		if(pa0 == 0)
			return -1;
		/* Sairaj:
		 *		This is the actual number of the bytes needs to be transfer
		 *		without considering internal fragmentation
		 */
		n = PGSIZE - (va - va0);
		if(n > len)
			n = len;
		/* Sairaj:
		 *		TODO: investigate why kernel space address is used for the
		 *		mapping
		 *		partial explaination: The caller processes' page table does not
		 *		have mapping for the virtual address for child process and we
		 *		are still using this table in the exec
		 *
		 *		Now in kernel address space mapping we are confirm that it will
		 *		have the mapping for it, since setupuvm maps all of physical
		 *		address to the kernel space.
		 */
		memmove(pa0 + (va - va0), buf, n);
		len -= n;
		buf += n;
		va = va0 + PGSIZE;
	}
	return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

