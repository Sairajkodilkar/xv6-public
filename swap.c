#include "swap.h"
#include "defs.h"
#include "spinlock.h"

/* SWAP structure:
 *		first block is swap header
 *		second block is swap bitmap
 *		third block onwards actual data blocks starts
 */

static struct swap_header sw;

static struct {
	uchar swap_bit_map[SWAP_BLOCK_SIZE];
	struct spinlock lock; 
} swap;

/* There is no need to store the bitmap on disk */

void init_swap() {
	sw.size = 100; /* TODO: read this from the file */
	sw.data_start = 1;
	swap.swap_bit_map[0] |= 1;
	/* Read the swap header from the swap space */
}

uint alloc_swap(void) {

	uint i, j;
	uint block_no;

	/* TODO: get the lock for the swap_bit_map */

	for(i = 0; i < SWAP_BLOCK_SIZE; i++) {
		j = 0;
		/* check if all bits are allocated in from the bitmap */
		if(swap.swap_bit_map[i] == ~1)
			continue;
		while(j < UCHAR_BITS) {
			block_no = (i * UCHAR_BITS) + j;
			if(block_no >= sw.size){ 
				return 0;
			}
			if((swap.swap_bit_map[i] & (1<<j)) == 0) {
				swap.swap_bit_map[i] |= (1<<j);
				return block_no;
			}
			j++;
		}
	}

	return 0;
}

void dealloc_swap(uint swap_block_no) {

	/* TODO: aquire the lock */
	uint index;
	uint offset;

	index = swap_block_no / UCHAR_BITS;
	offset = swap_block_no % UCHAR_BITS;

	swap.swap_bit_map[index] &= ~(1<<offset);

	return;
}

uint read_swap_block(char *buffer, uint swap_block) {
	/* read the swap block into the buffer pointed by the buffer */
	return 0;
}

uint write_swap_block(const char *buffer, uint block_no){
	return 0;
}

