/* name of program entry point label */
ENTRY(start)

/* flat binary output format */
OUTPUT_FORMAT(binary)

/* file sections */
SECTIONS
{
	. = 0x400000;
        .text : /* code section */
	{
		*(.text) /* take all .text sections from all input object files */
	}
       .rodata : /* read-only data section */
	{
		*(.rodata) /* take all .rodata sections from all input object files */
	}
	.data : /* data section  */
	{
		*(.data) /* take all .data sections from all input object files */
	}
	.bss : /* uninitialized statically allocated variable section */
	{
		*(COMMON) /* take all .COMMON sections from all input object files */
		*(.bss) /* take all .bss sections from all input object files */
	}
}
