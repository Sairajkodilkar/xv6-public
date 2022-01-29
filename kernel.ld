/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start) /* specify the starting point of the program */

SECTIONS
{
	/* Link the kernel at this address: "." means location counter*/
        /* Must be equal to KERNLINK */
	. = 0x80100000;

	/* output section */
	/* AT defines the loading address */
	.text : AT(0x100000) {
		/* star is a wildcard which matches to all input file
		 * i.e. all .text .stub etc sections in all input file must be in
		 * output section .text 
		 */
		*(.text .stub .text.* .gnu.linkonce.t.*)
	}

	/* Does not give error if program already has definition 
	 * If program does not have definition but only reference then this
	 * definition is used
	 */
	PROVIDE(etext = .);	/* Define the 'etext' symbol to this value */
	/* etext --> end text */

	/* readonly data */
	.rodata : {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}

	/* Include debugging information in kernel memory 
	 * How they went to the last ? in objdump -h output
	 */
	.stab : {
		PROVIDE(__STAB_BEGIN__ = .);
		*(.stab);
		PROVIDE(__STAB_END__ = .);
	}

	.stabstr : {
		PROVIDE(__STABSTR_BEGIN__ = .);
		*(.stabstr);
		PROVIDE(__STABSTR_END__ = .);
	}

	/* Adjust the address for the data segment to the next page */
	. = ALIGN(0x1000);

	/* Conventionally, Unix linkers provide pseudo-symbols
	 * etext, edata, and end, at the end of the text, data, and bss.
	 * For the kernel mapping, we need the address at the beginning
	 * of the data section, but that's not one of the conventional
	 * symbols, because the convention started before there was a
	 * read-only rodata section between text and data. 
	 */
	PROVIDE(data = .);

	/* The data segment */
	.data : {
		*(.data) /* Data sections of all the input file */
	}

	PROVIDE(edata = .);

	.bss : {
		*(.bss) /* BSS sections of all the input file */
	}

	PROVIDE(end = .);

	/DISCARD/ : {
		/* Discard these sections */
		*(.eh_frame .note.GNU-stack)
	}
}
