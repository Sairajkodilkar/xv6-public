#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"



void page_fault_intr() {

	uint pgflt_vaddr, pa;
	struct proc *curproc;
	struct disk_mapping *dm;
	struct inode *ip;

	pgflt_vaddr = rcr2();
	curproc = myproc();
	cprintf("PGFLT address %x, prog %d\n", pgflt_vaddr, curproc->pid);

	setptep(curproc->pgdir, pgflt_vaddr, 0);

	dm = find_disk_mapping(&(curproc->pdm), PGROUNDDOWN(pgflt_vaddr));
	if(dm < 0)
		panic("Mapping not found\n");


	begin_op();
	ip = namei(curproc->name);
	ilock(ip);

	pte_t *pte = getpte(curproc->pgdir, dm->vaddr, 0);
	//loaduvm(&(curproc->pdm), curproc->pgdir, (char *)dm->vaddr, ip, dm->offset, dm->size);
	readi(ip, dm->vaddr, dm->offset, dm->size);

	iunlockput(ip);
	end_op();
	ip = 0;

	return;
}
