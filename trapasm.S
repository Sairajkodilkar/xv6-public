#include "mmu.h"

  # vectors.S sends all traps here.
.globl alltraps
alltraps:
  # Build trap frame.
  pushl %ds
  pushl %es
  pushl %fs
  pushl %gs
  pushal
  
  # Set up data segments.
  /* Sairaj:
   * Until this point we are using the same data segment as that of the program 
   * This loads the data segment for the kernel 
   * No need to load the CS:EIP as it is already loaded by the hardware
   */
  movw $(SEG_KDATA<<3), %ax
  movw %ax, %ds
  movw %ax, %es

  # Call trap(tf), where tf=%esp
  pushl %esp //argument to the trap which is current trap frame
  call trap
  addl $4, %esp

  # Return falls through to trapret...
.globl trapret
trapret:
  popal
  popl %gs
  popl %fs
  popl %es
  popl %ds
  /* Now we skip the trapno and the errcode pushed by the vector.S and directly
   * go to the caller
   * Reason to skip this when process is forked it does not follow the vector.S
   * path of the interrupt
   * Second reason is that it is vector.S does not use "call" but use "jmp"
   */
  addl $0x8, %esp  # trapno and errcode
  iret # Sairaj: directly go to the caller 
