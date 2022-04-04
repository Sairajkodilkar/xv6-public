#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define FILE_SYSTEM_DEV (1)

static int load_fs_page(char *vaddr, const struct disk_mapping *dm) {
	struct inode *ip;

	if(get_dm_offset(dm) == INVALID_OFFSET)
		return 0;

	begin_op();
	ip = iget(FILE_SYSTEM_DEV, get_dm_inum(dm));
	ilock(ip);

	int count = readi(ip, vaddr, get_dm_offset(dm), get_dm_size(dm));

	iunlockput(ip);
	end_op();
	ip = 0;

	return count;
}

void page_fault_intr() {

	uint pgflt_vaddr;
	uint old_flags;
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

	pte = getpte(curproc->pgdir, (char *)get_dm_vaddr(dm));

	/* Allocate the memory */
	mem = kalloc();
	if(mem == 0) {
		panic("out of memory\n");
	}
	memset((void *)mem, 0, PGSIZE);
	old_flags = PTE_FLAGS(*pte);
	*pte = 0;
	*pte = V2P(mem) | PTE_P | old_flags;

	/* Read the ELF file */
	if(IS_SWAP_MAP(dm)) {
		cprintf("swapping\n");
		read_swap_block(mem, get_dm_block_num(dm));
	}
	else {
		cprintf("here\n");
		load_fs_page(mem, dm);
	}

	old_flags = get_dm_flags(dm);
	set_dm_flags(dm, old_flags|IN_MEM);
	/* TODO: dealloc swap */

	return;
}
