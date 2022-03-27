#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define FILE_SYSTEM_DEV (1)

void page_fault_intr() {

	uint pgflt_vaddr;
	char *mem;
	pte_t *pte;
	struct proc *curproc;
	struct disk_mapping *dm;
	struct inode *ip;

	pgflt_vaddr = rcr2();
	curproc = myproc();
	cprintf("PGFLT address %x, prog %d, prog name %s\n", pgflt_vaddr, curproc->pid, curproc->name);

	setptep(curproc->pgdir, (char *)pgflt_vaddr);

	dm = find_disk_mapping(&(curproc->pdm), PGROUNDDOWN(pgflt_vaddr));
	if(dm < 0)
		panic("Mapping not found\n");

	pte = getpte(curproc->pgdir, (char *)dm->vaddr);

	/* Allocate the memory */
	mem = kalloc();
	if(mem == 0) {
		panic("out of memory\n");
	}
	memset((void *)mem, 0, PGSIZE);
	*pte = 0;
	*pte = V2P(mem) | PTE_P | PTE_U | PTE_W;

	/* Read the ELF file */
	begin_op();
	ip = iget(FILE_SYSTEM_DEV, dm->inum);
	ilock(ip);
	readi(ip, mem, dm->offset, dm->size);
	iunlockput(ip);
	end_op();
	ip = 0;

	return;
}
