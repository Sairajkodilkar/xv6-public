#include "swap.h"

/* SWAP structure:
 *		first block is swap header
 *		second block is swap bitmap
 *		third block onwards actual data blocks starts
 */

static struct swap_header sw;


void init_swap() {

	struct buf swap_buffer;

	sw->swap_size = SWAP_SIZE;
	sw->block_size = BLOCK_SIZE;
	sw->total_allocated = 0;

	/*
	swap_buffer->flags = 0;
	swap_buffer->dev = 2;
	swap_buffer->blockno = 0;
	swap_buffer->refcnt = 1;
	*/

	//iderw(&swap_buffer);
}

uint alloc_swap(void) {
	return 0;
}

void dealloc_swap(uint swap_block_no) {
	return;
}

void read_swap(buffer, swap_block) {
	/* read the swap block into the buffer pointed by the buffer */
}


