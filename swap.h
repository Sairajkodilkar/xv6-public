#ifndef __SWAP_H
#define __SWAP_H

#include "types.h"

struct swap_header {
	uint size; /* total number of swap blocks */
	uint data_start; /* starting block of the data */
};

#define SWAP_DEV (2)
#define SWAP_BLOCK_SIZE (4096)

void init_swap(void);
uint alloc_swap(void);
void dealloc_swap(uint block_no);
uint read_swap_block(char *buffer, uint block_no);
uint write_swap_block(const char *buffer, uint block_no);
uint copy_swap_page(uint);

#endif
