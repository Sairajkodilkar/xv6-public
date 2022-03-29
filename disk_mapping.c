#include "types.h"
#include "disk_mapping.h"
#include "defs.h"
void map_to_disk(struct disk_mapping *dm, uint vaddr, 
		uint offset, uint flags, uint inum){

	dm->vaddr = vaddr;
	dm->flags = flags;

	if(IS_FILE_MAP(dm)) {
		dm->map.fm.offset = offset;
		dm->map.fm.inum = inum;
	}
	else {
		dm->map.sm.block_num = offset;
	}

	return;
}

