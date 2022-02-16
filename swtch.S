# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

.globl swtch
swtch:
/* Sairaj:
 * The calling convention saves the return address after the parameter
 * i.e. save CS and perform jump
 * Now we are sure that these are passed via stack because in GCC this is the
 * default behavior for the 32 bit machine
 */
  movl 4(%esp), %eax // &(c->scheduler)
  movl 8(%esp), %edx // context

  # Save old callee-saved registers
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks
  /* Sairaj:
   *
   * Scheduler:
   *	This stores the current stacks as the schedulers stack
   * and get the loads the process' esp inside
   *
   * Sced:
   *	The function sched does exactly the opposite and restores the
   *	schedulers stack
   */
  movl %esp, (%eax)
  movl %edx, %esp

  # Load new callee-saved registers
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  /* Sairaj: 
  Since scheduler schedules the process this return will go the IP stored on
   * stack of the process 
   * This will go back to the yield() then trap will return and then process
   * again goes to the user space 
   */
   /* Sairaj
    * When the process is newly created the eip on the context is pointing to
	* the the forkret
	* Hence this ret goes to the forkret
	*/
  ret
