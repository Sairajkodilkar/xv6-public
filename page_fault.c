#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


void page_fault_intr() {

	uint pgflt_vaddr, kvaddr;
	struct proc *curproc;
	struct disk_mapping *dm;
	struct inode *ip;

	pgflt_vaddr = rcr2();
	curproc = myproc();

	dm = find_disk_mapping(&(curproc->pdm), pgflt_vaddr);

	kvaddr = (uint) uva2ka(curproc->pgdir, (char *)dm->vaddr);

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
