#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "drive_mapping.h"
#include "file.h"

/* Sairaj:
 *	TODO: Load the program section wise
 */
int
exec(char *path, char **argv)
{
	char *s, *last;
	int i, off;
	uint argc, sz, sp, ustack[3+MAXARG+1], oldsz;
	struct elfhdr elf;
	struct inode *ip;
	struct proghdr ph;
	pde_t *pgdir, *oldpgdir;
	struct proc *curproc = myproc();
	uint flags;

	begin_op();

	if((ip = namei(path)) == 0){
		end_op();
		return -1;
	}
	ilock(ip);
	pgdir = 0;

	// Check ELF header
	if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
		goto bad;
	if(elf.magic != ELF_MAGIC)
		goto bad;

	if((pgdir = setupkvm()) == 0)
		goto bad;

	// Load program into memory.
	sz = 0;
	for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
		oldsz = sz;
		flags = PTE_U;
		if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
			goto bad;
		if(ph.type != ELF_PROG_LOAD)
			continue;
		if(ph.memsz < ph.filesz)
			goto bad;
		if(ph.vaddr + ph.memsz < ph.vaddr)
			goto bad;
		if(ph.flags | ELF_PROG_FLAG_WRITE)
			flags |= PTE_W;
		if((sz = allocuvm(pgdir, &(curproc->pv2dm), sz, ph.vaddr + ph.memsz, flags)) == 0)
			goto bad;

		update_proc_v2drive_map(&(curproc->pv2dm), oldsz, ph.vaddr + ph.memsz);

		map2file_range(&(curproc->pv2dm), oldsz, ph.vaddr + ph.memsz, ip->inum, ph.off);

		if(ph.vaddr % PGSIZE != 0)
			goto bad;
	}
	iunlockput(ip);
	end_op();
	ip = 0;

	// Allocate two pages at the next page boundary.
	// Make the first inaccessible.  Use the second as the user stack.
	sz = PGROUNDUP(sz);
	oldsz = sz;

	if((sz = allocuvm(pgdir, &(curproc->pv2dm), sz, sz + PGSIZE, PTE_P | PTE_W)) == 0)
		goto bad;

	if((sz = allocuvm(pgdir, &(curproc->pv2dm), sz, sz + PGSIZE, PTE_P | PTE_U | PTE_W)) == 0)
		goto bad;


	/* Map only the inaccessible page to the swap,
	 * Do not swap out the stack page as we need it to store the arguments
	 */
	map2swap_range(&(curproc->pv2dm), oldsz, PGSIZE);

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

	//swapout_vaddr(&(curproc->pv2dm), oldsz + PGSIZE);

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
	curproc->tf->esp = sp;
	switchuvm(curproc);
	freevm2(oldpgdir, &(curproc->pv2dm));
	return 0;

bad:
	if(pgdir)
		freevm2(pgdir, &(curproc->pv2dm));
	if(ip){
		iunlockput(ip);
		end_op();
	}
	return -1;
}
