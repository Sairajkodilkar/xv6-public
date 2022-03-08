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

void map2swap(struct v2drive_map *vdm, uint block_num){
	vdm->type = SWAP;
	vdm->drive.sm.block_num = block_num;
	return;
}
