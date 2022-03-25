#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"



void page_fault_intr() {

	uint pgflt_vaddr;
	struct proc *curproc;
	struct disk_mapping *dm;
	struct inode *ip;

	pgflt_vaddr = rcr2();
	curproc = myproc();

	if((dm = find_disk_mapping(&(curproc->pdm), PGROUNDDOWN(pgflt_vaddr))) == 0) {
		panic("mapping not found\n");
	}

	cprintf("%d\n", pgflt_vaddr);
	setptep(curproc->pgdir, dm->vaddr, 1);

	pte_t *pte = walkpgdir(curproc->pgdir, (void *)pgflt_vaddr, 0);
	cprintf("PTE ADDR %x\n", PTE_ADDR(*pte));
	if(pte == 0)
		panic("WTF\n");
	panic("NORMAL\n");

	/*
	begin_op();
	ip = namei(curproc->name);
	ilock(ip);
	cprintf("reading started\n");
	readi(ip, (char *)kvaddr, dm->offset, PGSIZE);
	cprintf("reading done\n");
	iunlockput(ip);
	end_op();
	*/

	panic("page fault\n");
}
