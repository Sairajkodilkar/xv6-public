//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "mmu.h"

struct devsw devsw[NDEV];
enum SlabState {EMPTY, PARTIAL, FULL};

struct slab_run {
	struct slab_run *next_slab;
	uint total_allocated;
};

struct {
  struct spinlock lock;
  struct slab_run *slab;
} ftable;

#define TOTAL_SLAB_OBJECTS ((4096 - sizeof(struct slab_run)) / sizeof(struct file))
void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

void slab_alloc(struct slab_run **slab) {
	  *slab = (struct slab_run *)kalloc();
	  memset(*slab, 0, 4096);
	  (*slab)->next_slab = 0;
	  (*slab)->total_allocated = 0;
}

struct file *get_free_object_from_slab(struct slab_run *slab) {
	if(slab->total_allocated == TOTAL_SLAB_OBJECTS)
		return 0;
	struct file *fp = (struct file *)(slab + 1);
	for(uint i = 0; i < TOTAL_SLAB_OBJECTS; fp++, i++) {
		if(fp->ref == 0) {
			fp->ref = 1;
			break;
		}
	}
	slab->total_allocated++;
	return fp;
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);

  if(ftable.slab == 0) {
	  slab_alloc(&ftable.slab);
  }
  struct slab_run *slab = ftable.slab;
  struct slab_run *prev_slab = 0;

  while(slab && slab->total_allocated == TOTAL_SLAB_OBJECTS) {
	  prev_slab = slab;
	  slab = slab->next_slab;
  }
  if(!slab) {
	  slab_alloc(&slab);
	  prev_slab->next_slab = slab;
  }

  f = get_free_object_from_slab(slab);

  release(&ftable.lock);
  return f;
}

// Increment ref count for file f.
	struct file*
filedup(struct file *f)
{
	acquire(&ftable.lock);
	if(f->ref < 1)
		panic("filedup");
	f->ref++;
	release(&ftable.lock);
	return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
	void
fileclose(struct file *f)
{
	struct file ff;

	acquire(&ftable.lock);
	if(f->ref < 1)
		panic("fileclose");
	if(--f->ref > 0){
		release(&ftable.lock);
		return;
	}

	struct slab_run *slab = (struct slab *)PGROUNDDOWN((uint) f);
	slab->total_allocated--;

	ff = *f;
	f->ref = 0;
	f->type = FD_NONE;
	release(&ftable.lock);

	if(ff.type == FD_PIPE)
		pipeclose(ff.pipe, ff.writable);
	else if(ff.type == FD_INODE){
		begin_op();
		iput(ff.ip);
		end_op();
	}
}

// Get metadata about file f.
	int
filestat(struct file *f, struct stat *st)
{
	if(f->type == FD_INODE){
		ilock(f->ip);
		stati(f->ip, st);
		iunlock(f->ip);
		return 0;
	}
	return -1;
}

// Read from file f.
	int
fileread(struct file *f, char *addr, int n)
{
	int r;

	if(f->readable == 0)
		return -1;
	if(f->type == FD_PIPE)
		return piperead(f->pipe, addr, n);
	if(f->type == FD_INODE){
		ilock(f->ip);
		if((r = readi(f->ip, addr, f->off, n)) > 0)
			f->off += r;
		iunlock(f->ip);
		return r;
	}
	panic("fileread");
}

//PAGEBREAK!
// Write to file f.
	int
filewrite(struct file *f, char *addr, int n)
{
	int r;

	if(f->writable == 0)
		return -1;
	if(f->type == FD_PIPE)
		return pipewrite(f->pipe, addr, n);
	if(f->type == FD_INODE){
		// write a few blocks at a time to avoid exceeding
		// the maximum log transaction size, including
		// i-node, indirect block, allocation blocks,
		// and 2 blocks of slop for non-aligned writes.
		// this really belongs lower down, since writei()
		// might be writing a device like the console.
		int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
		int i = 0;
		while(i < n){
			int n1 = n - i;
			if(n1 > max)
				n1 = max;

			begin_op();
			ilock(f->ip);
			if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
				f->off += r;
			iunlock(f->ip);
			end_op();

			if(r < 0)
				break;
			if(r != n1)
				panic("short filewrite");
			i += r;
		}
		return i == n ? n : -1;
	}
	panic("filewrite");
}

