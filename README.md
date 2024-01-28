# A hobbyist SMP OS for multi-core x64 CPUs written from scratch

## What this OS is and what it is not

My goal, when I set out to write this Operating System (OS), was to go through all the system programming need to boot and setup symmetric multi processing (SMP) on a real multi-core 64-bit x64 CPU from scratch and be able to run multiple user processes with very minimal support for Virtual Memory, hard disk and keyboard.
I did this solely for learning purposes and, as expected, the result ended up far from being a realistic or even just usable OS.
Nevertheless, all the code I wrote to boot the system up, enable 64-bit Long Mode, Virtual Memory, Interrupts, SMP, etc., while not state-of-the-art, is real, in the sense that no shortcuts have been taken and
it was tested on production level x64 emulators like QEMU and partly on real x64 machines.

My hope is that the code can be a useful reference for anyone who embarked on a journey to learn OS development from scratch, regardless of the complexity or realism of their implementation goals.

## Notes about the code
The best way to navigate the code and get a feel for how it works, is to try to understand the steps needed to boot up the OS by looking into the code/functions the kernel code executes to get the user processes up and running..
These are the main source files involved in the kernel boot-up process:

1. <code>src/boot/boot.asm</code> (512-byte 16-bit Real Mode bootloader, first sector of boot disk): reads second-stage bootloader from disk.
2. <code>src/boot/loader.asm</code> (512-byte second-stage bootloader, second sector of boot disk: gets CPU into 32-bit Protected Mode, reads kernel from disk, gets CPU into 64-bit Long Mode, jumps to kernel initialization code.
3. <code>src/boot/kernel.asm:</code> first part of kernel initialization code. It sets the Bootstrap Processor (BP) flag "BPFlag" for Application Processors (APs, basically the cores started after the first core, the BP, which enumerates and activates the other cores on the system) to skip over parts of the bootloader and initialization code that must only be run once by the BP (the address used for this flag is the address of the 2-byte magic word used by the BIOS to identify the boot sector: 0x7c00 + 510 in physical memory); it then jumps to the "C code entry point" in src/kernel.c..
3. <code>src/kernel.c</code>: initializes Virtual Memory, Interrupts, Global Descriptor Table (GDT), Task Segment Structure(TSS), LAPIC and IOAPIC, SMP, scheduler, VGA and start-up user processes.
4. The graphics branch contains some minimal PS2 mouse support and some pretty dirty and little-tested changes to <code>src/process.c</code> and <code>src/process.h</code> that all together implement a very rudimentary GUI based on the old VESA BIOS EXTENSIONS (VBE). Once the system is booted, new windows (each corresponding to processes spinning in an infinite loop) can be create by typing p followed by enter. Windows can be moved by left clicking on them, holding and dragging the mouse cursor.
### Comments
- The filesystem is a very minimal implementation of FAT16 that can only read files with file names not larger than 8 bytes from the root directory.
- The kernel size and the size of the example user processes loaded from disk at start-up are hard-coded. If code changes result in a change of the size of the binaries, the size variables in the code need to be changed accordingly.
- Some constants are declared twice: once for the assembly code and once for the C code. Do not forget to update both versions if a value changes (see constants defined <code>src/boot/defs.asm</code>).
- System calls are implemented using the syscall/sysret pair of instructions.
- There is no USB support and, therefore, no USB keyboard support. A PS2 keyboard or Legacy BIOS PS2 emulation are needed to run the kernel.

## Pre-requisites
You are going to need Linux or a Unix environment to build this OS.
The kernel is written in C and assembly. You need a GCC Cross-Compiler to compile it.

You can follow the steps described on this OSDev Wiki page (which are summarized below) to build one:
https://wiki.osdev.org/GCC_Cross-Compiler

This is the list of needed packages (names below are specific to Ubuntu):

1. build-essential
2. bison
3. flex
4. libgmp3-dev
5. libmpc-dev
6. libmpfr-dev
7. texinfo
8. libisl-dev

After installing the packages, create and move to a new directory to build gcc and binutils:
    
 	mkdir $HOME/src
 	cd $HOME/src

Download the source code archives for binutils and gcc. For instance:
    
 	wget https://ftp.gnu.org/gnu/binutils/binutils-2.39.tar.gz
 	wget http://mirrors.concertpass.com/gcc/releases/gcc-12.2.0/gcc-12.2.0.tar.gz

Decompress the archives:
    
 	tar -xvf binutils-2.39.tar.gz
 	tar -xvf gcc-12.2.0.tar.gz 

Create two build directories:
    
 	mkdir build-binutils
 	mkdir build gcc

Define the following enviroment variables (the cross-compiler will be installed in $PREFIX):
   
 	export PREFIX="$HOME/opt/cross"
 	export TARGET=x86_64-elf
 	export PATH="$PREFIX/bin:$PATH"

Build binutils:
   
 	cd build-binutils
 	../binutils-2.39/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
 	make
 	make install

Build gcc:
    
 	cd ../build-gcc
 	../gcc-12.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
 	make all-gcc
 	make all-target-libgcc
 	make install-gcc
 	make install-target-libgcc

## To build and run:

1. Install NASM: https://www.nasm.us/
2. Install QEMU https://www.qemu.org/
3. Build: 
	./build.sh
4. Run: 
	qemu-system-x86_64 -hda ./bin/os.bin -cpu qemu64,pdpe1gb -smp 2 -m 4G

## Debugging with gdb example
Run gdb and type following commands to get started: 
	
	add-symbol-file ./build/kernelfull.o 0x100000
	b _start
	target remote | qemu-system-x86_64 -hda ./bin/os.bin -cpu qemu64,pdpe1gb --smp 2 -m 4G S -gdb stdio
 	c
