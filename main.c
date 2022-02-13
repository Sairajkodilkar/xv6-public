#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
	/* The virtual entry is useful here */

	/* effectively allocates 746 pages */
  kinit1(end, P2V(4*1024*1024)); // phys page allocator
  kvmalloc();      // kernel page table
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  picinit();       // disable pic
  ioapicinit();    // another interrupt controller
  consoleinit();   // console hardware
  uartinit();      // serial port
  pinit();         // process table
  tvinit();        // trap vectors
  binit();         // buffer cache
  fileinit();      // file table
  ideinit();       // disk 
  startothers();   // start other processors
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  userinit();      // first user process
  mpmain();        // finish this processor's setup
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  idtinit();       // load idt register
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  scheduler();     // start running processes
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void(**)(void))(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

    lapicstartap(c->apicid, V2P(code));

    // wait for cpu to finish mpmain()
    while(c->started == 0)
      ;
  }
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.

/* can we use different page sizes for different page entry */

/* Sairaj:
 *
 * Any page with page dir address starting with 10 zeros or kernbase should map
 * to first frame in memory where kernel is residing
 * And we can definitly fit kernel in 4MB address space as
 * 0x7bab + 0xd4d0 = 0x1507b(86139 B) < 0x40000 
 *
 * The page directory and page table must be 4K align
 * thats why the 20 bits are enough for the representation
 * So we store only upper 20 bits in CR3
 *
 * TODO: investigate weather only upper 10 bits are used in case of the 4MB
 * pages
 *
 * IF ONLY the upper 10 bits are useful in the 4MB pages then why its not
 * working when lower 10 bits are changed!!!!!!!!!
 * ANS:
 *	If CR4.PSE = 1 and the PDE’s PS flag is 1, the PDE maps a 4-MByte page (see Table 4-4). The final physical
 *	address is computed as follows:
		— Bits 39:32 are bits 20:13 of the PDE.
		— Bits 31:22 are bits 31:22 of the PDE.
		— Bits 21:0 are from the original linear address.

	This is because intel uses 40 bit addressing 
	That's why my modification changed it
	The PSE=1 does not uses bit 12 and bit 21

	1. Bits in the range 39:32 are 0 in any physical address used by 32-bit 
	paging except those used to map 4-MByte pages. If the processor does not 
	support the PSE-36 mechanism, this is true also for physical addresses used 
	to map 4-MByte pages. If the processor does support the PSE-36 mechanism 
	and MAXPHYADDR < 40, bits in the range 39:MAXPHYADDR are 0 in any physical 
	address used to map a 4-MByte page. (The corresponding bits are reserved 
	in PDEs.) 
	See Section 4.1.4 for how to determine MAXPHYADDR and
	whether the PSE-36 mechanism is supported.

	2. The upper bits in the final physical address do not all come from 
	corresponding positions in the PDE; the physical-address bits in the
	PDE are not all contiguous
 *
 */
__attribute__((__aligned__(PGSIZE)))
	pde_t entrypgdir[NPDENTRIES] = {
		// Map VA's [0, 4MB) to PA's [0, 4MB)
		[0] = (0 | 1<<12) | PTE_P | PTE_W | PTE_PS,
		// Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
		[KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
	};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

