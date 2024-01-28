/* name of program entry point label */
ENTRY(_start)

/* flat binary output format */
OUTPUT_FORMAT(binary)

/* file sections */
SECTIONS
{
        gLocalApicAddress = 0xffff800000006000;
        gActiveCpuCount   = 0xffff800000006008;
        gNMemoryRegions   = 0xffff800000006010;
        gMemoryMap        = 0xffff800000006018;
	. = 0xffff800000200000; /* location counter (starting address) = higher-half kernel starting addr + 2M (0x200000) */	
	.text : ALIGN(4096) /* code section, align to 4096-byte boundary */
	{
		*(.text) /* take all .text sections from all input object files */
	}
	.rodata : ALIGN(4096) /* read-only data section, align to 4096-byte boundary */
	{
		*(.rodata) /* take all .rodata sections from all input object files */
	}
	.data : ALIGN(4096) /* data section, align to 4096-byte boundary */
	{
		*(.data) /* take all .data sections from all input object files */
	}
	.bss : ALIGN(4096) /* uninitialized statically allocated variable section, align to 4096-byte boundary */
	{
                PROVIDE(bssStart = .);
		*(COMMON) /* take all .COMMON sections from all input object files */
		*(.bss) /* take all .bss sections from all input object files */
                PROVIDE(bssEnd = .);
	}
        PROVIDE(kernelEnd = .);
}
