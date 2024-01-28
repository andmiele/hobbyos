FILES = ./build/kernel.asm.o ./build/kernel.o ./build/acpi/acpi.o ./build/idt/idt.asm.o ./build/io/io.asm.o ./build/idt/idt.o ./build/lib/lib.o ./build/memory/memory.asm.o ./build/memory/memory.o ./build/spinlock.asm.o ./build/stdio/stdio.o ./build/vga/vga.o ./build/graphics/graphics.o ./build/graphics/aerialFont.o ./build/graphics/gui.o  ./build/gdt/gdt.asm.o ./build/gdt/gdt.o ./build/process/process.o ./build/syscall/syscall.o ./build/syscall/syscall.asm.o ./build/drivers/keyboard.o  ./build/drivers/mouse.o ./build/drivers/disk.o ./build/fat16/fat16.o
USERSPACEFILES = ./build/userspace/start.asm.o ./build/userspace/syscall.asm.o ./build/userspace/stdio.o ./build/userspace/stdlib.o
USERPROGRAMS = ./build/userspace/user1.o ./build/userspace/shell.o ./build/userspace/user2.o ./build/userspace/test.o ./build/userspace/ls.o

INCLUDES = -I./src
FLAGS = -g -mno-red-zone -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc -mcmodel=large
# -g: generate debug information to be used by GDB debugger
# -mno-red-zone: The "red zone" is a System V ABI optimization that allows functions to temporarily use the 128 bytes below their stack frame without adjusting the stack pointer
# -falign-...: align to power of 2 boundary
# -fstrength-reduce: loop strength reduction and elimination of iteration variables optimizations
# -fomit-frame-pointer: do not keep frame pointer in a register for functions that do not need it
# -finline-functions: inline simple functions
# --freestanding: standard library may not exist, program startup may not be at "main" (disable built-in functions)
# -Wno-unused-functions: disable warning for unused functions
# -fno-builtin: do not recognize built-in functions that do not begin with __builtin as a prefix
# -Werror: turn warnings into errors
# -Wno-unused-label: suppress warnings for declared labels that are not used
# -Wno-cpp: suppress warnings emitted by #warning directives
# -Wno-unused-parameter: suppress warnings for function parameters that are unused aside from their declaration 
# -nostdlib: do not link any libraries or startup files by default
# -nostartfiles: do not use the standard system startup files when linking
# -nodefaultlibs: do not use the standard system libraries when linking
# -Wall: enable all compiler's warning messages
# -O0: no implicit optimizations to make debugging easier
# -Iinc: search for source files included with #include in inc directory
# -relocatble: link all object files so that the output can in turn serve as input to ld
# -mcmodel=large: Places no memory restriction on code or data. All accesses of code and data must be done with absolute addressing

all: ./bin/boot.bin ./bin/loader.bin ./bin/kernel.bin ./bin/user1.bin ./bin/shell.bin ./bin/user2.bin ./bin/test.bin ./bin/ls
	rm -f ./bin/os.img
	dd if=./bin/boot.bin >> ./bin/os.img
	dd if=./bin/loader.bin >> ./bin/os.img
	dd if=./bin/kernel.bin conv=sync >> ./bin/os.img
# this is just to print the size of the system user processes loaded at startup
	dd if=./bin/user1.bin conv=sync >> /dev/null
	dd if=./bin/user2.bin conv=sync >> /dev/null
	dd if=./bin/shell.bin conv=sync >> /dev/null
	dd if=/dev/zero bs=1048576 count=16 >> ./bin/os.img
	#MacOS
	hdiutil attach bin/os.img
	cp {bin/user1.bin,bin/shell.bin,bin/user2.bin,TEST.TXT,bin/test.bin,bin/ls} /Volumes/Untitled 
	hdiutil detach /Volumes/Untitled
	#Linux
	#sudo mount -t vfat bin/os.img ./disk
	#sudo cp ./bin/user1.bin ./bin/shell.bin ./bin/user2.bin ./bin/test.bin ./bin/ls TEST.TXT ./disk
	#sudo umount ./disk

./bin/kernel.bin: $(FILES) ./src/linker.ld
	x86_64-elf-ld -relocatable $(FILES) -o ./build/kernelfull.o
	#x86_64-elf-gcc $(FLAGS) -T ./src/linker.ld -o ./bin/kernel.bin ./build/kernelfull.o
	x86_64-elf-ld -T ./src/linker.ld -o ./bin/kernel.bin ./build/kernelfull.o

./bin/boot.bin: ./src/boot/boot.asm ./src/boot/defs.asm
	nasm -f bin ./src/boot/boot.asm -o ./bin/boot.bin

./bin/loader.bin: ./src/boot/loader.asm ./src/boot/defs.asm	
	nasm -f bin ./src/boot/loader.asm -o ./bin/loader.bin

./build/kernel.asm.o: ./src/kernel.asm ./src/linker.ld ./src/boot/defs.asm
	nasm -f elf64 -g ./src/kernel.asm -o ./build/kernel.asm.o

./build/kernel.o: ./src/kernel.c ./src/kernel.h
	x86_64-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/kernel.c -o ./build/kernel.o

./build/acpi/acpi.o: ./src/acpi/acpi.c ./src/acpi/acpi.c
	x86_64-elf-gcc $(INCLUDES) -I./src/acpi $(FLAGS) -std=gnu99 -c ./src/acpi/acpi.c -o ./build/acpi/acpi.o

./build/gdt/gdt.o: ./src/gdt/gdt.c ./src/gdt/gdt.h
	x86_64-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/gdt/gdt.c -o ./build/gdt/gdt.o

./build/gdt/gdt.asm.o: ./src/gdt/gdt.asm ./src/boot/defs.asm
	nasm -f elf64 -g ./src/gdt/gdt.asm -o ./build/gdt/gdt.asm.o

./build/idt/idt.o: ./src/idt/idt.c ./src/idt/idt.h
	x86_64-elf-gcc $(INCLUDES) -I./src/idt $(FLAGS) -std=gnu99 -c ./src/idt/idt.c -o ./build/idt/idt.o

./build/idt/idt.asm.o: ./src/idt/idt.asm ./src/boot/defs.asm
	nasm -f elf64 -g ./src/idt/idt.asm -o ./build/idt/idt.asm.o

./build/io/io.asm.o: ./src/io/io.asm ./src/boot/defs.asm
	nasm -f elf64 -g ./src/io/io.asm -o ./build/io/io.asm.o

./build/lib/lib.o: ./src/lib/lib.c ./src/lib/lib.h
	x86_64-elf-gcc $(INCLUDES) -I./src/lib $(FLAGS) -std=gnu99 -c ./src/lib/lib.c -o ./build/lib/lib.o

./build/memory/memory.o: ./src/memory/memory.c ./src/memory/memory.h
	x86_64-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/memory/memory.c -o ./build/memory/memory.o

./build/memory/memory.asm.o: ./src/memory/memory.asm ./src/boot/defs.asm
	nasm -f elf64 -g ./src/memory/memory.asm -o ./build/memory/memory.asm.o

./build/process/process.o: ./src/process/process.c ./src/process/process.h
	x86_64-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/process/process.c -o ./build/process/process.o

./build/spinlock.asm.o: ./src/spinlock.asm ./src/boot/defs.asm
	nasm -f elf64 -g ./src/spinlock.asm -o ./build/spinlock.asm.o

./build/stdio/stdio.o: ./src/stdio/stdio.c ./src/stdio/stdio.h
	x86_64-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/stdio/stdio.c -o ./build/stdio/stdio.o

./build/syscall/syscall.o: ./src/syscall/syscall.c ./src/syscall/syscall.h ./src/syscall/syscall.asm
	x86_64-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/syscall/syscall.c -o ./build/syscall/syscall.o

./build/syscall/syscall.asm.o: ./src/syscall/syscall.asm ./src/boot/defs.asm
	nasm -f elf64 -g ./src/syscall/syscall.asm -o ./build/syscall/syscall.asm.o

./build/vga/vga.o: ./src/vga/vga.c ./src/vga/vga.h
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/vga/vga.c -o ./build/vga/vga.o

./build/graphics/aerialFont.o: ./src/graphics/aerialFont.c ./src/graphics/aerialFont.h
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/graphics/aerialFont.c -o ./build/graphics/aerialFont.o

./build/graphics/graphics.o: ./src/graphics/graphics.c ./src/graphics/graphics.h 
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/graphics/graphics.c -o ./build/graphics/graphics.o

./build/graphics/gui.o: ./src/graphics/gui.c ./src/graphics/gui.h 
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/graphics/gui.c -o ./build/graphics/gui.o


./build/drivers/keyboard.o: ./src/drivers/keyboard.c ./src/drivers/keyboard.h
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/drivers/keyboard.c -o ./build/drivers/keyboard.o

./build/drivers/mouse.o: ./src/drivers/mouse.c ./src/drivers/mouse.h
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/drivers/mouse.c -o ./build/drivers/mouse.o

./build/drivers/disk.o: ./src/drivers/disk.c ./src/drivers/disk.h
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/drivers/disk.c -o ./build/drivers/disk.o

./build/fat16/fat16.o: ./src/fat16/fat16.c ./src/fat16/fat16.h
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/fat16/fat16.c -o ./build/fat16/fat16.o

./build/userspace/syscall.asm.o: ./src/userspace/syscall.asm ./src/boot/defs.asm
	nasm -f elf64 -g ./src/userspace/syscall.asm -o ./build/userspace/syscall.asm.o

./build/userspace/start.asm.o: ./src/userspace/start.asm
	nasm -f elf64 -g ./src/userspace/start.asm -o ./build/userspace/start.asm.o

./build/userspace/stdio.o: ./src/userspace/stdio.c ./src/userspace/stdio.h
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/userspace/stdio.c -o ./build/userspace/stdio.o

./build/userspace/stdlib.o: ./src/userspace/stdlib.c ./src/userspace/stdlib.h
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/userspace/stdlib.c -o ./build/userspace/stdlib.o

./build/userspace/user1.o: ./src/userspace/user1.c
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/userspace/user1.c -o ./build/userspace/user1.o
./build/userspace/shell.o: ./src/userspace/shell.c
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/userspace/shell.c -o ./build/userspace/shell.o
./build/userspace/user2.o: ./src/userspace/user2.c
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/userspace/user2.c -o ./build/userspace/user2.o
./build/userspace/test.o: ./src/userspace/test.c
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/userspace/test.c -o ./build/userspace/test.o
./build/userspace/ls.o: ./src/userspace/ls.c
	x86_64-elf-gcc $(FLAGS) -std=gnu99 -c ./src/userspace/ls.c -o ./build/userspace/ls.o


# The ar utility creates and maintains groups of files combined into an archive.  Once an archive has been created, new files can be added and existing files can be extracted, deleted, or replaced
# CREATES STATIC LIBRARY!
# -r: Replace or add the specified files to the archive.  If the archive does not exist a new archive file is created.  Files that replace existing files do not change the order of the files within
# the archive.  New files are appended to the archive unless one of the options -a, -b or -i is specified.

# -c: Whenever an archive is created, an informational message to that effect is written to standard error. If the -c option is specified, ar creates the archive silently 

# -s: Write an object-file index into the archive, or update an existing one, even if no other change is made to the archive. You may use this modifier flag either with any operation, or alone.
# Running `ar s' on an archive is equivalent to running `ranlib' on it. 

#./build/userspace/lib.a: ./build/userspace/syscall.asm.o ./build/userspace/stdio.o
#	ar rcs ./build/userspace/lib.a ./build/userspace/syscall.asm.o ./build/userspace/stdio.o

./bin/user1.bin: $(USERSPACEFILES) ./src/userspace/linker.ld ./build/userspace/user1.o 
	x86_64-elf-ld -relocatable  $(USERSPACEFILES) ./build/userspace/user1.o -o ./build/user1.o
	x86_64-elf-gcc $(FLAGS) -T ./src/userspace/linker.ld -o ./bin/user1.bin ./build/user1.o
./bin/shell.bin: $(USERSPACEFILES) ./src/userspace/linker.ld ./build/userspace/shell.o
	x86_64-elf-ld -relocatable  $(USERSPACEFILES) ./build/userspace/shell.o -o ./build/shell.o
	x86_64-elf-gcc $(FLAGS) -T ./src/userspace/linker.ld -o ./bin/shell.bin ./build/shell.o
./bin/user2.bin: $(USERSPACEFILES) ./src/userspace/linker.ld ./build/userspace/user2.o
	x86_64-elf-ld -relocatable  $(USERSPACEFILES) ./build/userspace/user2.o -o ./build/user2.o
	x86_64-elf-gcc $(FLAGS) -T ./src/userspace/linker.ld -o ./bin/user2.bin ./build/user2.o
./bin/test.bin: $(USERSPACEFILES) ./src/userspace/linker.ld ./build/userspace/test.o
	x86_64-elf-ld -relocatable  $(USERSPACEFILES) ./build/userspace/test.o -o ./build/test.o
	x86_64-elf-gcc $(FLAGS) -T ./src/userspace/linker.ld -o ./bin/test.bin ./build/test.o
./bin/ls: $(USERSPACEFILES) ./src/userspace/linker.ld ./build/userspace/ls.o
	x86_64-elf-ld -relocatable  $(USERSPACEFILES) ./build/userspace/ls.o -o ./build/ls.o
	x86_64-elf-gcc $(FLAGS) -T ./src/userspace/linker.ld -o ./bin/ls ./build/ls.o

clean:
	rm -f ./bin/*
	rm ./build/*.o
	rm -f $(FILES)
	rm -f $(USERSPACEFILES)
	rm -f $(USERPROGRAMS)
