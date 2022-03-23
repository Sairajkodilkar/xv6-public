#ifndef __DISK_MAPPING
#define __DISK_MAPPING
enum MapType {FILE_MAP, SWAP_MAP};

struct disk_mapping {
	uint vaddr;
	enum MapType type;
	char name[16];
	uint offset;
};

#endif
