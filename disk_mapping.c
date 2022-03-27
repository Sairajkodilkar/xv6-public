#include "types.h"
#include "disk_mapping.h"
#include "defs.h"
void map_to_disk(struct disk_mapping *dm, uint vaddr, 
		uint offset, enum MapType type, uint inum){

	dm->vaddr = vaddr;
	dm->type = type;

	if(type == FILE_MAP) {
		dm->map.fm.offset = offset;
		dm->map.fm.inum = inum;
	}
	else {
		dm->map.sm.block_num = offset;
	}

	return;
}

