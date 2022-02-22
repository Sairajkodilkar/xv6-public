#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

/* NOTE:
 *		here kernel stack does not change as it has to return to the trap
 *		whereas in fork the kernel stack changes
 *
 *		Steps:
 *			1) Get the executable file inode
 *			2) Read the ELF file header
 *			3) read the program header and alocate the user memory to it
 *			4) load the corrosponding section into the allocated region
 *
 *			5) Allocate 2 Free pages, mark 1 as NOT user 
 *			6) Now initiate the user stack from the 1 of the allocated pages
 *			7) Load the arguments, argv, argc, and PC on the stack
 *			8) change the trap frame of the process
 *			9) switch enviroment
 *			10) free the old virtual memory 
 */
int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  /* Sairaj:
   *	get the inode for the given path 
   */
  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  /* Sairaj:
   *	Check if the file is at least of the size of ELF
   */
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  /* Sairaj:
   *	Setup the kernel virtual memory
   */
  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
	  /* Sairaj:
	   *	Read the program header from the given offset off inside the inode
	   *	ip having the size of sizeof(ph) and store it in the memory ph
	   *	onwards
	   */
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
	/* Sairaj:
	 *	In case of stack the ph.vaddr and ph.memsz both are zero henc the
	 *	condition newsz < oldsz becomes true and return sz is just oldsz
	 */
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
	/* Sairaj:
	 * IF the virtual address is not page align then it a error
	 * TODO: investiagte Why ?
	 * Also why not check this before calling allocuvm ?
	 * because loaduvm requires that the address must be page align
	 */
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
	/* Sairaj:
	 *	Actually loads the program from the disk to the memory
	 */
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  /* sz is the highest virtual address allocated in the user memory space
   */
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  /* Sairaj
   * clear the PTE_U flag of the page between the data and the stack segment
   * This makes the page inacessible to the user
   */
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
	/* Sairaj:
	 *	decrement the stack pointer in the multiple of the 4 bytes only
	 */
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
	/* Sairaj:
	 *		This copies the bytes of length form the argv[argc] to the sp
	 */
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
	/* Sairaj:
	 *	Inside the program we need to provide the pointer to these arguments
	 *	since user program uses it as char **
	 *	so we need the address for each of these arguments on the stack also
	 *	hence store it
	 *	
	 *	NOW why 3?
	 *	Remember the calling conventions:
	 *	parameter
	 *	return address
	 *			call --> this will store the 4 byte EIP on the stack
	 */
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC 
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer
	
  /* Sairaj
   * |argv		 |
   * |argc		 |
   * |return PC  |
   * |_ _ _ _ _ _|
   */

  /* Sairaj:
   * 3 = argv argc and return
   * argc = total number of arguments pointers
   * 1 = last NULL pointer to mark the end of pointers
   *		Here total elements are multiplied by the 4 since each address
   *		length is 4 byte.
   */
  sp -= (3+argc+1) * 4;
  /* Sairaj:
   * simply copy this info on the stack
   */
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp; /* Sairaj: This is the user stack defined by the exec */
  switchuvm(curproc);
  freevm(oldpgdir);
  /* Sairaj:
   *	Where will this return go and which path does it follow 
   *	This will go to the syscall->trap->trapret->new_process
   */
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
