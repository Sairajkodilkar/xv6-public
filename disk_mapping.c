#include "types.h"
#include "disk_mapping.h"
#include "defs.h"
void map_to_disk(struct disk_mapping *dm, uint vaddr, uint offset, enum MapType type){
	cprintf("mapping address %x\n", vaddr);
	dm->vaddr = vaddr;
	dm->type = type;
	dm->offset = offset;
	return;
}
