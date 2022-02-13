//
// assembler macros to create x86 segments
//
//Format:
//	Limit low
//	base low		#lower 2 bytes of the base |
//	base middle		#middle 1 byte of the base |
//	access									   |--> total 4 bytes = 32 bit base
//	granularity #flag and higher 4 bit limit		   |
//	base high		#higher 1 byte of the base |
//
//80286 MANUAL:
//
//	When G = 0, the actual limit is the value of the 20-bit limit field as it 
//	appears in the descriptor. In this case, the limit may range from 0 to 
//	0FFFFFH (220 - 1 or 1 megabyte). When G= 1, the processor appends 12 
//	low-order one-bits to the value in the limit field. In this case the 
//	actual limit may range from 0FFFH (212 - 1 or 4 kilobytes) to 0FFFFFFFFH 
//	(232 - 1 or 4 gigabytes).
//
//	(USE the unit of 4K instead of 1 byte )
//	

#define SEG_NULLASM                                             \
        .word 0, 0;                                             \
        .byte 0, 0, 0, 0

// The 0xC0 means the limit is in 4096-byte units
// and (for executable segments) 32-bit mode.
#define SEG_ASM(type,base,lim)                                  \
        .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);      \
        .byte (((base) >> 16) & 0xff), (0x90 | (type)),         \
                (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#define STA_X     0x8       // Executable segment
#define STA_W     0x2       // Writeable (non-executable segments)
#define STA_R     0x2       // Readable (executable segments)
