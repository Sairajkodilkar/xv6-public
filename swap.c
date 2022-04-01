#include "swap.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"

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
	uint blockno;

	/* TODO: get the lock for the swap_bit_map */

	for(i = 0; i < SWAP_BLOCK_SIZE; i++) {
		j = 0;
		/* check if all bits are allocated in from the bitmap */
		if(swap.swap_bit_map[i] == ~1)
			continue;
		while(j < UCHAR_BITS) {
			blockno = (i * UCHAR_BITS) + j;
			if(blockno >= sw.size){ 
				return 0;
			}
			if((swap.swap_bit_map[i] & (1<<j)) == 0) {
				swap.swap_bit_map[i] |= (1<<j);
				return blockno;
			}
			j++;
		}
	}

	return 0;
}

void dealloc_swap(uint swap_blockno) {

	/* TODO: aquire the lock */
	uint index;
	uint offset;

	index = swap_blockno / UCHAR_BITS;
	offset = swap_blockno % UCHAR_BITS;

	swap.swap_bit_map[index] &= ~(1<<offset);

	return;
}

void init_swap_buf(struct buf *swap_buf, uchar *data, uint blockno, uint flags) {

	swap_buf->flags = 0;
	swap_buf->dev = SWAP_DEV;
	swap_buf->blockno = blockno;
	swap_buf->bsize = 512;
	swap_buf->data = data;
	swap_buf->refcnt = 1;

	initsleeplock(&(swap_buf->lock), "swap buffer");

	return;
}

uint read_swap_block(char *data, uint blockno) {
	/* read the swap block into the buffer pointed by the buffer */
	struct buf swap_buf;

	init_swap_buf(&swap_buf, data, blockno, 0);

	acquiresleep(&(swap_buf.lock));
	iderw(&swap_buf);

	return SWAP_BLOCK_SIZE;
}

uint write_swap_block(const char *data, uint blockno){

	struct buf swap_buf;

	init_swap_buf(&swap_buf, data, blockno, B_DIRTY);

	acquiresleep(&(swap_buf.lock));
	iderw(&swap_buf);

	return SWAP_BLOCK_SIZE;
}

