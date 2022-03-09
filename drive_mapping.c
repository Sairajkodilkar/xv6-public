#include "types.h"
#include "drive_mapping.h"

void assign_virtual_addr(struct v2drive_map *vdm, uint vaddr){
	vdm->vaddr = vaddr;
}

void map2file(struct v2drive_map *vdm, uint inum, uint offset){
	vdm->type = FILE_SYSTEM;
	vdm->drive.fsm.inum = inum;
	vdm->drive.fsm.offset = offset;
	return;
}

void map2swap(struct v2drive_map *vdm, uint block_num[BPP]){
	vdm->type = SWAP;
	/* TODO: use mem move */
	for(int i = 0; i < BPP; i++)
		vdm->drive.sm.block_num[i] = block_num[i];
	return;
}
