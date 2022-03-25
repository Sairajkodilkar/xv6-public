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

	/*
	if((dm = find_disk_mapping(&(curproc->pdm), PGROUNDDOWN(pgflt_vaddr))) == 0) {
		panic("mapping not found\n");
	}
	*/

	cprintf("%d\n", pgflt_vaddr);
	setptep(curproc->pgdir, pgflt_vaddr, 0);

	dm = find_disk_mapping(&(curproc->pdm), PGROUNDDOWN(pgflt_vaddr));
	if(dm < 0)
		panic("Mapping not found\n");


	begin_op();
	ip = namei(curproc->name);
	ilock(ip);

	pte_t *pte = walkpgdir(curproc->pgdir, dm->vaddr, 0);
	//loaduvm(&(curproc->pdm), curproc->pgdir, (char *)dm->vaddr, ip, dm->offset, dm->size);
	readi(ip, dm->vaddr, dm->offset, dm->size);
	cprintf("vaddr: %x and pa: %x\n", dm->vaddr, PTE_ADDR(*pte));

	iunlockput(ip);
	end_op();
	ip = 0;

	/*
	   pte_t *pte = walkpgdir(curproc->pgdir, (void *)dm->vaddr, 0);
	   if(pte == 0)
	   panic("ABNORMAL\n");

	   pa = PTE_ADDR(*pte);
	   */

	/* TODO add offset and size in the disk mapping field */
	/*
	   begin_op();
	   ip = namei(curproc->name);
	   ilock(ip);
	   if(readi(ip, P2V(pa), dm->offset, PGSIZE) != PGSIZE) {
	   panic("fork failing\n");
	   }
	   iunlockput(ip);
	   end_op();
	   */
	cprintf("PAGE FLT SERVE\n");

	//panic("page fault\n");
}
