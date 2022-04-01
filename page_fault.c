#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define FILE_SYSTEM_DEV (1)

static int load_disk_page(char *vaddr, const struct disk_mapping *dm) {
	struct inode *ip;
	begin_op();
	ip = iget(FILE_SYSTEM_DEV, get_dm_inum(dm));
	ilock(ip);

	int count = readi(ip, vaddr, get_dm_offset(dm), get_dm_size(dm));

	iunlockput(ip);
	end_op();
	ip = 0;

	return count;
}

static int load_swap_page(uint page_vaddr) {
	panic("load swap page: Not yet implemented\n");
	return 0;
}

char x[4096] = "sairaj Kodilkar";

void page_fault_intr() {

	uint pgflt_vaddr;
	char *mem;
	pte_t *pte;
	struct proc *curproc;
	struct disk_mapping *dm;

	pgflt_vaddr = rcr2();
	curproc = myproc();
	cprintf("PGFLT address %x, prog %d, prog name %s\n", pgflt_vaddr, curproc->pid, curproc->name);

	setptep(curproc->pgdir, (char *)pgflt_vaddr);

	dm = find_disk_mapping(&(curproc->pdm), PGROUNDDOWN(pgflt_vaddr));
	if(dm < 0)
		/* TODO: in future kill the program */
		panic("Mapping not found\n");

	if(IS_SWAP_MAP(dm)) {
		panic("Swap yet to be implemented");
	}

	pte = getpte(curproc->pgdir, (char *)get_dm_vaddr(dm));

	/* Allocate the memory */
	mem = kalloc();
	if(mem == 0) {
		panic("out of memory\n");
	}
	memset((void *)mem, 0, PGSIZE);
	*pte = 0;
	*pte = V2P(mem) | PTE_P | PTE_U | PTE_W;

	/* Read the ELF file */
	load_disk_page(mem, dm);

	return;
}
