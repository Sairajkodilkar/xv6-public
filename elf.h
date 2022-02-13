// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint magic;  // must equal ELF_MAGIC
  uchar elf[12]; 
  ushort type; /* Object file type relocatble, executable, etc */
  ushort machine; /* machine ISA type */
  uint version; /* 1 for original version of elf */
  uint entry; /* entry point where the process start executing */
  uint phoff; /* Points to the start of the program header */
  uint shoff; /* section header */
  uint flags;
  ushort ehsize; /* Size of this header */
  ushort phentsize; /* Size of each program header table entry */
  ushort phnum; /*number of entries in program header */
  ushort shentsize; /*size of section header table entry */
  ushort shnum; /* number of entries in section header */
  ushort shstrndx; /* dont know */
};

// Program section header
struct proghdr {
  uint type; /* Identifies the type of segment */
  uint off; /* offset of the segment in the file image */
  uint vaddr; /* virtual address of the segment in the memory */
  uint paddr; /* physical address of the segment */
  uint filesz; /* size of segment in bytes in file image */
  uint memsz; /*  size of seg in bytes in memory */
  uint flags;
  uint align; /* when greater than 1: vaddr = off % align */
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4
