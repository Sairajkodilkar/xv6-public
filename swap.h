#ifndef __SWAP_H
#define __SWAP_H

#include "types.h"

struct swap_header {
	uint swap_size;
	uint block_size;
	uint total_allocated;
};

uint alloc_swap(void);
void dealloc_swap(uint block_no);
uint read_swap_block(const char *buffer, uint block_no);
uint write_swap_block(char *buffer, uint block_no);

#endif
