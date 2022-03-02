#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
	/* Sairaj: How may data blocks are logged */
  int n;
  /* Sairaj: This contains the block number for which we have logged data
   * i.e the given ith block from the log start corrosponds to the block[i]th
   * block of the data/disk
   */
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  /* Sairaj: Stating block of the log */
  int start;
  /* Sairaj: Size of the log */
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  /* Sairaj:
   *	The device number where the log is stored
   */
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

/* Sairaj:
 *	Read the super block
 *	initialize the global log structure using this super block
 *	perform the disk recovery from the existing log the disk
 */
void
initlog(int dev)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  /*Sairaj:
   *	The super block contains the meta data about the file system
   *	This is stored on the first block of the file
   */
  readsb(dev, &sb);
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(void)
{
  int tail;

  /* Sairaj:
   *	Read the n log blocks starting from the log 
   *	Read the corrospoinding block from the harddisk
   *		-This step is important since we have all the transactions through
   *		the buffers
   *	Transfer data from the log buffer to the destionation buffer
   *	Now perform the bwrite on the destination 
   *
   *	TODO: investigate Why we skip the first block
   *		Because the first block contains the log header
   */
  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
	/* Sairaj:
	 *	Read all the logs to the destinationation blocks
	 */
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
	/* Sairaj:
	 *		Who is aquiring the lock
	 */
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
/* Sairaj:
 * The first block of the log is the logheader
 * Read that log header and initialize the log structure
 */
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
	/*Sairaj: This reads the buffer */
  read_head();
  /* Sairaj: This copies the blocks from the log to the destination block  */
  install_trans(); // if committed, copy from log to disk
  /* Sairaj:
   *	Why not set the log.lh.n inside the install transactions ?
   *	May be because transaction should be done in its entirety
   */
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
	  /* Sairaj:
	   *	Sleep until the log is commiting the data
	   */
    if(log.committing){
      sleep(&log, &log.lock);
	  /* Sairaj:
	   *	LOGSIZE is 30
	   *	MAXOPBLOCKS is 10
	   */
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
		/* Sairaj:
		 *	There are log.lh.n blocks
		 *	each remaining can utilize max 10 blocks
		 *	hence multiply outstanding + 1 by 10( here 1 is for the current
		 *	process 
		 *	if that count execeeds we simply sleep 
		 */
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  /* Sairaj:
   *	The log should not be commiting as we have already checked it during
   *	the begin op
   */
  if(log.committing)
    panic("log.committing");
  /* Sairaj:
   *	Wait till all the syscalls are over
   *	Wont this create a deadlock as we have already acquired the log lock
   *	and other syscall wont be able to aquire the log lock and decrease the
   *	outstanding
   */
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
/* Sairaj:
 * This writes the block which is inside the buffer to the log area
 * starting from the block log header + 1
 * This is buffer to buffer transfer
 *
 * bwrite ensures that the buffer is written on the log part of the disk
 */
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
	  /*Sairaj:
	   *	Why read from start + 1
	   */
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
	/*Sairaj:
	 *	Write the modified header data having the info about written log blocks
	 *	is written to the header block
	 */
    write_head();    // Write header to disk -- the real commit
    install_trans(); // Now install writes to home locations
	/* Sairaj:
	 *	Since we have written the data to the exact place we can empty the
	 *	blocks
	 */
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
/*Sairaj:
 *	This only records the block number of the given buffer inside the block
 *	array
 *	actual log write is done by write_log
 *	PS: Naming is very bad
 */
void
log_write(struct buf *b)
{
  int i;

  /* Sairaj: log.size is the number of the log mentioned in the super block
   */
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  /* Sairaj: This condition enforces to use the begin OP at the start of the
   *		File IO
   */
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  /* Sairaj:
   *	Record the block number of the buffer
   */
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n)
    log.lh.n++;
  b->flags |= B_DIRTY; // prevent eviction
  release(&log.lock);
}

