struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  /*Sairaj: Offset with in the file */
  uint off;
};


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  /* Sairaj:
   * The major number identifies the driver associated with the device
   */
  short major;

  /* Sairaj:
   *	The minor number is used only by the driver specified by the major number
   */
  short minor;

  /* Sairaj:
   *	number of directory entries that refer to this file
   */
  short nlink;
  /* Sairaj:
   *	Number of bytes of content in the file
   */
  uint size;
  /* Sairaj: 
   *	block number of the disk block holding the inode 
   *	The first NDIRECT blocks are directly mapped meaning there block number
   *	is directly stored onto the addrs array
   *	but the last bloc specifies the indirectl mapping i.e block numbers are
   *	stored on the block whos block no. is stored on the address
   */
  uint addrs[NDIRECT+1];
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
