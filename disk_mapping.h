#ifndef __DISK_MAPPING
#define __DISK_MAPPING
enum MapType {FILE_MAP, SWAP_MAP};

struct disk_mapping {
	uint vaddr;
	enum MapType type;
	uint offset;
};
void map_to_disk(struct disk_mapping *dm, uint vaddr, uint offset, enum MapType type);

#endif
