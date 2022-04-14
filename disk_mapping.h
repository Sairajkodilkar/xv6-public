#ifndef __DISK_MAPPING
#define __DISK_MAPPING

#define FREE		(0)
#define MAPPED		(1)
#define SWAP_MAP	(MAPPED << 1)
#define FILE_MAP	(SWAP_MAP << 1)
#define IN_MEM		(FILE_MAP << 1)

#define IS_FREE(dm)		(!(dm->flags))
#define IS_MAPPED(dm)	(dm->flags & MAPPED)
#define IS_FILE_MAP(dm) (dm->flags & FILE_MAP)
#define IS_SWAP_MAP(dm) (dm->flags & SWAP_MAP)
#define IS_IN_MEM(dm)	(dm->flags & IN_MEM)

#define INVALID_OFFSET	(-1)

struct file_map {
	int offset; /* offset in case of file system*/
	uint inum; /* inode number of the file */
	uint size; /* actual on disk size */
};

struct swap_map {
	uint block_num; /* block number of the swap */
};

struct disk_mapping {
	uint vaddr; /* page virtual address */
	uint flags; /* map flags */
	union {
		struct file_map fm;
		struct swap_map sm;
	} map;
	struct disk_mapping *prev, *next;
};

void map_to_disk(struct disk_mapping *dm, uint vaddr, int offset,
		uint flags, uint inum);

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
get_dm_flags(const struct disk_mapping *dm) {
	return dm->flags;
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
set_dm_offset(struct disk_mapping *dm, int offset) {
	dm->map.fm.offset = offset;
}

static inline void
set_dm_inum(struct disk_mapping *dm, uint inum) {
	dm->map.fm.inum = inum;
}

static inline void
set_dm_flags(struct disk_mapping *dm, uint flags) {
	dm->flags = flags;
}

static inline void
set_dm_vaddr(struct disk_mapping *dm, uint vaddr) {
	dm->vaddr = vaddr;
}

static inline void
set_dm_block_no(struct disk_mapping *dm, uint block_no) {
	dm->map.sm.block_num = block_no;
}
#endif
