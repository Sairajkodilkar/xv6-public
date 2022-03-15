#include "types.h"
#include "x86.h"
#include "defs.h"
#include "drive_mapping.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "file.h"
#include "memlayout.h"

pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc);
uint getpgflags(pde_t *pgdir, void *va);

void page_fault_intr(void) {

	struct inode *ip = 0;
	struct proc *curproc = myproc();
	char *mem; 
	uint uflags;


	uint linear_addr = rcr2();

	cprintf("page fault address: %p %p\n", linear_addr, curproc->tf->err);
	cprintf("page fault program: %s\n", curproc->name);

	pte_t *y = walkpgdir(curproc->pgdir, (void *)linear_addr, 0);

	struct v2drive_map *x = get_v2drive_map(&(curproc->pv2dm), PGROUNDDOWN(linear_addr));

	mem = kalloc();
	memset(mem, 0, PGSIZE);

	uflags = getpgflags(curproc->pgdir, (void *)x->vaddr);
	begin_op();
	ip = namei(curproc->name);
	ilock(ip);

	//cprintf("loading addr: %p\n at offset %d\n", x->vaddr, x->drive.fsm.offset);
	mappages(curproc->pgdir, (void *)x->vaddr, PGSIZE, V2P(mem), PTE_P | uflags);

	if(loaduvm(curproc->pgdir, (char *)x->vaddr, ip, x->drive.fsm.offset, PGSIZE) < 0) {
		panic("not working");
	}

	iunlockput(ip);
	end_op();
	ip = 0;

	//panic("Page fault\n");

	return;

}
