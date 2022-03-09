#ifndef __DRIVE_MAPPING_H 
#define __DRIVE_MAPPING_H

#include "mmu.h"
#include "fs.h"

#define BPP (PGSIZE / BSIZE)

enum drive_type {FILE_SYSTEM, SWAP};

struct file_system_map {
	uint inum;
	uint offset;
};

struct swap_map {
	uint block_num[BPP];
};

struct v2drive_map {
	uint type;
	uint vaddr;
	union {
		struct file_system_map fsm;
		struct swap_map sm;
	} drive;
};

void assign_virtual_addr(struct v2drive_map *vdm, uint vaddr);

void map2file(struct v2drive_map *vdm, uint inum, uint offset);

void map2swap(struct v2drive_map *vdm, uint block_num[BPP]);

#endif
