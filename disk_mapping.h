#ifndef __DISK_MAPPING
#define __DISK_MAPPING

enum MapType {FILE_MAP, SWAP_MAP};

struct file_map {
	uint offset; /* offset in case of file system*/
	uint inum; /* inode number of the file */
	uint size; /* actual on disk size */
};

struct swap_map {
	uint block_num; /* block number of the swap */
};

struct disk_mapping {
	uint vaddr; /* page virtual address */
	enum MapType type; /* map type */
	union {
		struct file_map fm;
		struct swap_map sm;
	} map;
};

void map_to_disk(struct disk_mapping *dm, uint vaddr, uint offset, 
		enum MapType type, uint inum);

static inline uint
get_dm_size(const struct disk_mapping *dm) {
	return dm->map.fm.size;
}

static inline uint 
get_dm_offset(const struct disk_mapping *dm) {
	return dm->map.fm.offset;
}

static inline uint
get_dm_inum(const struct disk_mapping *dm) {
	return dm->map.fm.inum;
}

static inline uint 
get_dm_block_num(const struct disk_mapping *dm) {
	return dm->map.sm.block_num;
}

static inline uint
get_dm_type(const struct disk_mapping *dm) {
	return dm->type;
}

static inline uint 
get_dm_vaddr(const struct disk_mapping *dm) {
	return dm->vaddr;
}

/* Setters */
static inline void 
set_dm_size(struct disk_mapping *dm, uint size) {
	dm->map.fm.size = size;
}

static inline void 
set_dm_offset(struct disk_mapping *dm, uint offset) {
	dm->map.fm.offset = offset;
}

static inline void 
set_dm_inum(struct disk_mapping *dm, uint inum) {
	dm->map.fm.inum = inum;
}

static inline void 
set_dm_type(struct disk_mapping *dm, enum MapType type) {
	dm->type = type;
}

static inline void 
set_dm_vaddr(struct disk_mapping *dm, uint vaddr) {
	dm->vaddr = vaddr;
}

static inline void 
set_dm_block_no(struct disk_mapping *dm, uint vaddr) {
	dm->map.sm.block_num = vaddr;
}
#endif
