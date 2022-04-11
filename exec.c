#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "disk_mapping.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "swap.h"

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint flags;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  uint inum;
  uint swap_blockno;
  uint old_flags;
  char *page_addr;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();
  struct disk_mapping *dm;

  struct disk_mapping *tmp;
  tmp = curproc->head;

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  inum = get_inum(ip);

  if((pgdir = setupkvm()) == 0)
    goto bad;


  // Load program into memory.
  struct proc_disk_mapping new_pdm;
  init_proc_disk_mapping(&new_pdm);
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
	  flags = PTE_U | PTE_P | PTE_W;
	  if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
		  goto bad;
	  if(ph.type != ELF_PROG_LOAD)
		  continue;
	  if(ph.memsz < ph.filesz)
		  goto bad;
	  if(ph.vaddr + ph.memsz < ph.vaddr)
		  goto bad;

	  flags &= ~PTE_P;

	  proc_map_to_disk(&new_pdm, sz, ph.vaddr + ph.memsz, INVALID_OFFSET, FILE_MAP, inum);

	  if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz, flags)) == 0)
		  goto bad;

	  if(ph.vaddr % PGSIZE != 0)
		  goto bad;

	  if(loaduvm(&new_pdm, pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
		  goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);

  /* here swap block no is passed as 0 because its not in memory */
  proc_map_to_disk(&new_pdm, sz, sz + PGSIZE, 0, IN_MEM | SWAP_MAP, -1);

  if((sz = allocuvm(pgdir, sz, sz + PGSIZE, PTE_W)) == 0)
	  goto bad;

  proc_map_to_disk(&new_pdm, sz, sz + PGSIZE, 0, SWAP_MAP, -1);

  if((sz = allocuvm(pgdir, sz, sz + PGSIZE, PTE_P|PTE_U|PTE_W)) == 0)
	  goto bad;

  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
	  if(argc >= MAXARG)
		  goto bad;
	  sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
	  if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
		  goto bad;
	  ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
	  goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
	  if(*s == '/')
		  last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  page_addr = (char *)(sz -  PGSIZE);
  dm = find_disk_mapping(&new_pdm, (uint)page_addr);

  swap_blockno = swap_out_page(pgdir, page_addr);
  set_dm_block_no(dm, swap_blockno);

  old_flags = get_dm_flags(dm);
  set_dm_flags(dm, old_flags & ~IN_MEM);

  curproc->pages_in_memory = 0;

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  clear_proc_disk_mapping(&(curproc->pdm));
  copy_pdm(&curproc->pdm, &new_pdm);
  freevm(oldpgdir);
  return 0;

bad:
  if(pgdir) {
	  clear_proc_disk_mapping(&(curproc->pdm));
	  clear_proc_disk_mapping(&new_pdm);
	  freevm(pgdir);
  }
  if(ip){
	  iunlockput(ip);
	  end_op();
  }
  return -1;
}
