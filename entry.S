# The xv6 kernel starts executing in this file. This file is linked with
# the kernel C code, so it can refer to kernel symbols such as main().
# The boot block (bootasm.S and bootmain.c) jumps to entry below.
        
# Multiboot header, for multiboot boot loaders like GNU Grub.
# http://www.gnu.org/software/grub/manual/multiboot/multiboot.html
#
# Using GRUB 2, you can boot xv6 from a file stored in a
# Linux file system by copying kernel or kernelmemfs to /boot
# and then adding this menu entry:
#
# menuentry "xv6" {
# 	insmod ext2
# 	set root='(hd0,msdos1)'
# 	set kernel='/boot/kernel'
# 	echo "Loading ${kernel}..."
# 	multiboot ${kernel} ${kernel}
# 	boot
# }

#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"

# Multiboot header.  Data to direct multiboot loader.
.p2align 2
.text
.globl multiboot_header
/* TODO: investigate this */
multiboot_header:
  #define magic 0x1badb002
  #define flags 0
  .long magic
  .long flags
  .long (-magic-flags)

# By convention, the _start symbol specifies the ELF entry point.
# Since we haven't set up virtual memory yet, our entry point is
# the physical address of 'entry'.
# 
# NOTE: this is specified in the linker script as the entry point

.globl _start
_start = V2P_WO(entry)

# Entering xv6 on boot processor, with paging off.
.globl entry
entry:
  # Turn on page size extension for 4Mbyte pages
  movl    %cr4, %eax
  /* Sairaj:
   *	This just tells that you can use 4MB pages if you want
   *	But the actually it is desided by the PS flag in the page directory
   *	entry.
   */
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  # Set page directory
  # Why we are loading lower bits inside entrypgdir as they are reserved for the
  # process context switch
  # shouldnt it be left shifted by 12 ?
  /* Why we do not OR the entrypgdir while storing in CR3
   * because pcd and pwt are not that imp and tasa pn zeroch ahet te
   */
  movl    $(V2P_WO(entrypgdir)), %eax
  /* EIP still has the address in physical format
   * hence the 0th entry is useful here
   */
  movl    %eax, %cr3
  # Turn on paging.
  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # Set up the stack pointer.
  movl $(stack + KSTACKSIZE), %esp

  # Jump to main(), and switch to executing at
  # high addresses. The indirect call is needed because
  # the assembler produces a PC-relative instruction
  # for a direct jump.
  # NOTE: The immediate operand in the absolute jump is illegal that's why we
  #		have to store it in the eax
  #		jmp *$main -->illegal

  # INVESTIGATE:
  # why this instruction is not valid
  #		jmp *main
  #		PC = [main] but we want PC = main
  # because it is equivalent to fetching address store at the main and jumping
  # to it 
  # movl main, %eax
  # jmp *%eax

  mov $main, %eax
  /* NOTE: This jump is with the paging hardware enabled hence the addresses
   * will be translated from 0x8010000 to 0x0010000 by hardware
   *	1) If I remove the star in th *%eax the program works fine with a small
   * warning from the assembler
   *	2) without the dollar the label is interpreted as the address and its
   *		it dereference to get the value
   */
  jmp *%eax
  /* JUMPS to the VIRTUAL address of the main 
   * Here that second entry comes into picture 
   */

/*
*.comm name, size, alignment
*
*    The .comm directive allocates storage in the data section. The storage is
*	referenced by the identifier name. Size is measured in bytes and must be a
*	positive integer. Name cannot be predefined. Alignment is optional. If
*	alignment is specified, the address of name is aligned to a multiple of
*	alignment.
*/
.comm stack, KSTACKSIZE
