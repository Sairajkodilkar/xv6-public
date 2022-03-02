// Simple PIO-based (non-DMA) IDE driver code.
/* TODO: during the swap space addition we need to select the second channel as
 * there are only 2 disks per channel
 */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80 //device is busy
#define IDE_DRDY      0x40 //0: when drive is spun down or error occurs, otherwise 1
#define IDE_DF        0x20 //drive fault error
#define IDE_ERR       0x01 //error

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock;
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf*);

// Wait for IDE disk to become ready.
static int
idewait(int checkerr)
{
  int r;

  /* Sairaj:
   *	reading from the 0x1f7 gives us status
   *	if the status is ready or not
   */
  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  /* Sairaj:
   *	check the error that user desiers
   */
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

void
ideinit(void)
{
  int i;

  initlock(&idelock, "ide");
  ioapicenable(IRQ_IDE, ncpu - 1);
  idewait(0);

  // Check if disk 1 is present
  /* 0x1f6 register is use to select the drive head register
   *	0x08: 4th bit selects the drive, since there are only two drives per channel
   *	it is enough
   *	0xe0: bits 5 and 7 are always set, 6th bit tells that use LBA
   *	addressing and not the CHS addressing
   */
  outb(0x1f6, 0xe0 | (1<<4));
  for(i=0; i<1000; i++){
	  /* Sairaj:
	   *	Reading from this register gives us status
	   */
    if(inb(0x1f7) != 0){
      havedisk1 = 1;
      break;
    }
  }

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));
}

// Start the request for b.  Caller must hold idelock.
/* Sairaj:
 *	Algorithm to read and write the buffer
 *		1) wait for the ide to free
 *		2) set the controller to generate the interrupt when operation
 *		completes to the register 0x3f6
 *		3) write the number of sectors to the 0x1f[2-5]
 *		4) select the driver and device using 0x1f6
 *		5) set the IDE to write mode using 0x1f7
 *		6) write the data from the port 0x1f0
 */
static void
idestart(struct buf *b)
{
  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  /* Sairaj:
   *	TODO: investigate why there can't be more than 7 sectors per block
   */
  if (sector_per_block > 7) panic("idestart");

  idewait(0);
  /* Sairaj:
   *	control register is set entirely zero
   *	when 2nd bit is 0 we set the interrupt
   */
  outb(0x3f6, 0);  // generate interrupt
  /* Sairaj:
   *	This registers stores the number of sectors to read and write
   *	What is the difference between block and sector
   */
  outb(0x1f2, sector_per_block);  // number of sectors
  /* Sairaj:
   *	Store the lower byte of the start sector number
   */
  outb(0x1f3, sector & 0xff);
  /* Sairaj:
   *	Store the middle byte of the start sector number
   */
  outb(0x1f4, (sector >> 8) & 0xff);
  /* Sairaj:
   *	Store the higher byte of the start sector number
   */
  outb(0x1f5, (sector >> 16) & 0xff);

  /* Sairaj:
   *		Select the drive to be given by the user
   *		the 0x1f6 also stores the upper 3 bits of the sector address
   *		and 0xe0 tells to use the LBA addressing
   */
  outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
  if(b->flags & B_DIRTY){
    outb(0x1f7, write_cmd);
	/* Sairaj:
	 *		The port 0x1f0 is use to read and write the data
	 *		write 4 bytes at a time with repeat to the data register
	 */
    outsl(0x1f0, b->data, BSIZE/4);
  } else {
    outb(0x1f7, read_cmd);
  }
}

// Interrupt handler.
void
ideintr(void)
{
  struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock);

  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  // Read data if needed.
  /* Sairaj:
   *	We set the read command during idestart, which generates the interrupt
   *	which is handled here and the data is read
   */
  if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  // Start disk on next buf in queue.
  if(idequeue != 0)
    idestart(idequeue);

  release(&idelock);
}

//PAGEBREAK!
// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
/*Sairaj:
 *		Append the buffer at the end of the queue
 *		sleep on the channel
 */
void
iderw(struct buf *b)
{
  struct buf **pp;

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");
  if(b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  acquire(&idelock);  //DOC:acquire-lock

  // Append b to idequeue.
  b->qnext = 0;
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue
    ;
  *pp = b;

  // Start disk if necessary.
  /* Sairaj:
   *	This is necessary as if the other ios are pending this means that the
   *	it will run idestart on the next buffer
   *	but if this is first IO in queue we need to start the ide
   */
  if(idequeue == b)
    idestart(b);

  // Wait for request to finish.
  /*Sairaj:
   *	This flag for the buffer is set by the interrupt
   *	hence to check if the interrupt has been successful or not
   *	but why not just perform sleep since only sucessful interrupt can 
   *	wake it up
   */
  while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
	  /* Sairaj: 
	   *	This sleep is wake uped by the ideintr
	   */
    sleep(b, &idelock);
  }


  release(&idelock);
}
