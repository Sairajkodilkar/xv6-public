#include "types.h"
#include "x86.h"
void page_fault_intr() {
	cprintf("page fault address %x\n", rcr2());

	uint vaddr;
	struct proc;

	vaddr = rcr2();

	panic("page fault\n");
}
