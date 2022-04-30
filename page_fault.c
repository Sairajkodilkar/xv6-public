#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "swap.h"

#define FILE_SYSTEM_DEV (1)

static int load_fs_page(char *vaddr, const struct disk_mapping *dm) {
	struct inode *ip;

	if(get_dm_offset(dm) == INVALID_OFFSET) {
		return 0;
	}
	begin_op();
	ip = iget(FILE_SYSTEM_DEV, get_dm_inum(dm));
	ilock(ip);

	int count = readi(ip, vaddr, get_dm_offset(dm), get_dm_size(dm));

	iunlockput(ip);
	end_op();
	ip = 0;

	return count;
}

static char *alloc_mem(pte_t *pte) {
	uint old_flags;
	char *mem;

	mem = kalloc();
	if(mem == 0) {
		panic("out of memory\n");
	}
	memset((void *)mem, 0, PGSIZE);
	old_flags = PTE_FLAGS(*pte);
	*pte = 0;
	*pte = V2P(mem) | PTE_P | old_flags;

	return mem;
}


void page_fault_intr() {

	uint pgflt_vaddr;
	uint swap_blockno;
	uint old_flags;
	char *mem;
	pte_t *pte;
	struct proc *curproc;
	struct disk_mapping *dm;

	pgflt_vaddr = rcr2();
	curproc = myproc();
	cprintf("PGFLT address %x, prog %d, prog name %s\n", PGROUNDDOWN(pgflt_vaddr), 
			curproc->pid, curproc->name);

	acquire(&curproc->pdm_lock);

	if(curproc->pages_in_memory > MAX_PAGES) {
		do {
			dm = &(curproc->pdm.proc_mapping[curproc->next_page++]);
		} while(!IS_IN_MEM(dm));
		cprintf("Swapping out %x\n", dm->vaddr);

		release(&curproc->pdm_lock);
		swap_blockno = swap_out_page(curproc->pgdir, (char *)(dm->vaddr));
		acquire(&curproc->pdm_lock);

		set_dm_block_no(dm, swap_blockno);
		set_dm_flags(dm, MAPPED | SWAP_MAP);
		curproc->pages_in_memory--;
	}

	setptep(curproc->pgdir, (char *)pgflt_vaddr);

	dm = find_disk_mapping(&(curproc->pdm), PGROUNDDOWN(pgflt_vaddr));

	if(dm == 0) {
		cprintf("page fault intr: killed %s\n", curproc->name);
		curproc->killed = 1;
		panic("page fault\n");
		return;
	}

	pte = getpte(curproc->pgdir, (char *)get_dm_vaddr(dm));

	mem = alloc_mem(pte);
	curproc->pages_in_memory++;

	release(&curproc->pdm_lock);

	if(IS_SWAP_MAP(dm)) {
		read_swap_block(mem, get_dm_block_num(dm));
		dealloc_swap(get_dm_block_num(dm));
	}
	else {
		load_fs_page(mem, dm);
	}

	acquire(&curproc->pdm_lock);

	old_flags = get_dm_flags(dm);
	set_dm_flags(dm, old_flags|IN_MEM);

	release(&curproc->pdm_lock);

	return;
}
