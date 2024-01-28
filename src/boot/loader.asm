; Copyright 2024 Andrea Miele
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at

;   http://www.apache.org/licenses/LICENSE-2.0

; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

; Second Stage Bootloader

; Real mode memory map
; 0x00000 - 0x003ff: Interrupt Vector Table
; 0x00400 - 0x004ff: BIOS Data Area, 256 bytes
; 0x00500 - 0x8ffff: Free memory
; 0x90000 - 0x9ffff: Extended BIOS Data Area (normally)
; 0xa0000 - 0xbffff: VGA Frame Buffer
; 0xc0000 - 0xc7fff: Video BIOS, 32K
; 0xc8000 - 0xeffff: Free memory
; 0xf0000 - 0xfffff: Motherboard BIOS, 64K
 
; 512-byte bootsector is loaded into:
; 0x07c00 - 0x07e00

; 512-byte loader (second sector) copied from disk to:
; 0x07e00 - 0x07d00

%include "src/boot/defs.asm"

PROT_MODE_CODE_SEG equ GDTCodeSegDesc - GDTStart 
PROT_MODE_DATA_SEG equ GDTDataSegDesc - GDTStart 

BITS 16                                         ; tell assembler we are in 16-bit mode
ORG 0x7e00                                      ; address at which BIOS loads the bootloader is 0x7c00

; if this is AP jump to enterProtectedMode

        mov dh, byte[BPFlag]                    ; load BP flag
        cmp dh, 0x1                             ; compare flag in dh to 1
        jz  .enterProtectedMode 	        ; jump to kernel if AP
        mov word[FAT16ReservedSectors], cx      ; cx contains FAT16 reserved sector number that counts the first 2 sectors (boot sector and second sector containing loader) and kernel image sectors
;        mov si, bootMsg		        ; put bootMsg string address in si
;        call printString16			; call 16-bit string print function

.enterProtectedMode:
	cli					; disable interrupts as we change segment registers
        lgdt [GDTDescriptorStruct]              ; load GDT descriptor struct
        mov eax, cr0				; move control register 0 content into eax
        or eax, 0x1                             ; set Protected mode bit
        mov cr0, eax                            ; update control register 0

	mov ax, PROT_MODE_DATA_SEG
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        
        mov dh, byte[BPFlag]                    ; load BP flag
        cmp dh, 0x1                             ; compare flag in dh to 1
        jz jumpKernel32AP 			; if AP skip kernel load and protected/long mode setup
        
; Enter protected mode
        jmp PROT_MODE_CODE_SEG:loadKernel32     

jumpKernel32AP:        
        jmp PROT_MODE_CODE_SEG:enterLongMode     

jumpKernelAP:

[BITS 32]
loadKernel32:
	mov eax, 2				; starting sector, LBA address, (skip boot sector and second sector: loader)
	mov ecx, [FAT16ReservedSectors]		; load number of FAT16 reserved sectors in ecx
        sub ecx, 2				; subtract 2 to obtain number of kernel image sectors
	mov edi, 0x00200000			; 2MB
        mov ebx, PROT_MODE_DATA_SEG             ; ebx = data segment selector 
        mov es, ebx	                        ; load es with data segment selector
        call ataLBARead				; call LBA disk read routine
loadUserspace:					; each userspace process is loaded at 128K (0x20000) + 64K (0x10000) * processIndex
        ; process 1
	xor ebp, ebp
	mov edi, 0x20000			; 128KB
        mov esi, [FAT16ReservedSectors]	        ; starting sector, LBA address, (skip reserved sectors)
userspaceLoop:
        cmp ebp, N_USER_PROCESSES		; check if all userspace processes have been loaded
	je userspaceDone
        ;mov ebx, PROT_MODE_DATA_SEG             ; ebx = data segment selector 
        ;mov es, ebx	                        ; load es with data segment selector
	mov ecx, N_USERSPACE_DISK_SECTORS	; number of sectors to read
        mov eax, esi                            ; starting sector, LBA address, (skip boot sector and previous userspace processes)
        call ataLBARead				; call LBA disk read routine
        sub edi, 512 * N_USERSPACE_DISK_SECTORS
        add edi, 0x10000			; update destination address for next userspace process: add 64KB to edi
        add esi, N_USERSPACE_DISK_SECTORS       ; update starting sector, LBA address, (skip boot sector and previous userspace processes)
        inc ebp
        jmp userspaceLoop 
userspaceDone:
        mov ebp, 0x00200000                     ; set base pointer at 2 MB offset
        mov esp, ebp                            ; set stack pointer at 2 MB offset

; Initialize Programmable Interrupt Timer (PIT)
; We set up the PIT to raise an interrupt 100 times per second (100 Hz)
; The PIT working frequency is 1.193182 MHz

; I/O port     Usage
; 0x40         Channel 0 data port (read/write)
; 0x41         Channel 1 data port (read/write)
; 0x42         Channel 2 data port (read/write)
; 0x43         Mode/Command register (write only)

; Mode/Command register 0x43
; Bits         Usage
; 6:7   Select channel: 00 = ch 0, 01 = ch 1, 10 = ch, 11 = read-back command (8254 only)
; 4:5   Access mode: 00 = latch count command, 01 =low byte, 10 = high byte, 11 = low byte / high byte
; 1:3   Operating mode :
;               000 = Mode 0 (interrupt on terminal count)
;               001 = Mode 1 (hardware re-triggerable one-shot)
;               010 = Mode 2 (rate generator)
;               011 = Mode 3 (square wave generator)
;               100 = Mode 4 (software triggered strobe)
;               101 = Mode 5 (hardware triggered strobe)
;               110 = Mode 2 (rate generator, same as 010b)
;               111 = Mode 3 (square wave generator, same as 011b)
; 0             BCD/Binary mode: 0 = 16-bit binary, 1 = four-digit BCD

initPIT:
mov al, 00110100b                 		; channel 0, low byte / high byte, rate generator, binary
out 0x43, al					; write command

; We set up the PIT to raise an interrupt 100 times per second (100 Hz)
; The PIT working frequency is 1.193182 MHz so we write 11931182 / 100 = 11931
; to channel 0 data port 0x40

mov ax, 11931
out 0x40, al 					; write low byte first
mov al, ah
out 0x40, al                                    ; write high byte


; The Intel 286 could access up to 16MB of memory (instead of 1 MB as the 8086). 
; To be compatible with software that used the 1MB segment:offset wrap-around trick on the 8086,
; the wrap-around behavior had to be replicated on the IBM AT PC. 
; This was obtained by disabling the A20 line (20th address bus line) by default

; Use fast A20 gate to enable A20 line
; Only supported starting with the IBM PS/2 PC or later
; WARNING: this might not be supported on some systems

enableA20line:                              
	in al, 0x92                             ; legacy programmed IO; read from port 0x92
	or al, 2                                ; set 2nd bit
	out 0x92, al                            ; legacy programmed IO; write al to port 0x92
 
setupPaging:
; Set up paging before entering Long Mode
; There are 4 levels (optionally 5 levels if supported by the CPU and enabled in which case there is a PML5T)
; in the page directory tree
; In long mode there are 512 8-byte entries per table/level
; PML4T: page-map level 4 table, one entry addresses 512GB (total of 256TB at most)
; PDPT: page-directory pointer table, one entry addresses 1GB
; PDT: page-directory table, one entry addresses 2MB
; PT: page table, one entry addresses 4KB

        cld					; clear direction flag: if DF flag is set to 0, string operations increment index registers (ESI and/or EDI)
; clear memory region (set to 0) that will contain the page tables
 	mov edi, 0x70000			; address of buffer for page tables = address of PML4T
	xor eax,eax				; eax = 0
	mov ecx, 0x10000 / 4			; size of buffer to be set to zero in double words 1M / 4
    	rep stosd				; repeat {[edi] = eax; edi = edi + 4; ecx = ecx - 1;} until ecx == 0

; last 12 bits of page table entry are flags: 11-9: Available (AVL),
; 8: Global (G), 7: Reserved (RSVD) for PML4T(PML5T) or Page Size (PS) for PDT and Page Size(PS) or Page Attribute Table (PAT) for PT, 6: Dirty (D), 
; 5: Accessed (A), 4: Cache Disable (PCD), 3: Write-through (PWT),
; 2: Supervisor/User (S/U), 1: Read/Write (R/W), 0: Present (P)

; set up PML4T
; We identity map first 4 GBs of physical memory to 4 1GB pages
        mov dword[0x70000], 0x00071003		; address of PDPT is 0x7100; last 12 bits are flags:
						; supervisor access, writable, present
        mov dword[0x71000], 0x00000083          ; physical address of page set to 0; last 12 bits are flags:
						; PS = 1: 1GB page, so PDPT contains actual page tables and PDT and PT are not used; supervisor, writable, present
        mov dword[0x71008], 0x40000083          ; physical address of page set to 1GB; last 12 bits are flags:
						; PS = 1: 1GB page, so PDPT contains actual page tables and PDT and PT are not used; supervisor, writable, present
        mov dword[0x71010], 0x80000083          ; physical address of page set to 2GB; last 12 bits are flags:
						; PS = 1: 1GB page, so PDPT contains actual page tables and PDT and PT are not used; supervisor, writable, present
        mov dword[0x71018], 0xc0000083          ; physical address of page set to 3GB; last 12 bits are flags:
						; PS = 1: 1GB page, so PDPT contains actual page tables and PDT and PT are not used; supervisor, writable, present

; We virtually map kernel space (right now kernel code is loaded at physical address 0x10000) starting from first x86-64 canonical virtual address (assuming 48-bit addresses are used) with msb set, namely 0xffff800000000000, so that:
; 0xFFFFFFFFFFFFFFF - 0xffff800000000000: KERNEL SPACE (e.g., kernel code loaded at physical address 0x10000 will be mapped to start address 0xffff800000010000)
; 0xFFFF7FFFFFFFFFF - 0x0000800000000000: NON-CANONICAL ADDRESSES
; 0x00007FFFFFFFFFF - 0x0000000000000000: USER SPACE
; Each entry in the PML4T table addresses 512GB (2^{39}) so we need to populate the (0xffff800000000000 / 2^{39})-th entry with physical address 0x10000 

       	mov eax, (0xffff800000000000 >> 39)	; PML4T entry number
        and eax, 0x1ff				; entry number modulo 512 (number of 8-byte PML4T entries)
	mov dword[0x70000 + eax * 8], 0x72003	; load PDPT address and flags: supervisor, writable, present
        mov dword[0x72000], 0x00000083          ; physical address of page set to 0; last 12 bits are flags:
						; PS = 1: 1GB page, so PDPT contains actual page tables and PDT and PT are not used; supervisor, writable, present

; set up 8259 PIC
setupPIC:
; initialization word 1 (ICW1): bit 4 (INIT), bit 3 (LEVEL: Edge/Level triggered mode),
; bit 2 (INTERVAL: call address interval 8/4), bit 1 (SINGLE: Cascade/Single mode),
; bit 0 (ICW4 not used/used)

; initialization word 4 (ICW4): bit 4 (SFNM: Special fully nested disable/enable), 
; bit 3 (BUF MASTER: Buffered mode master disable/enable)
; bit 2 (BUF MASTER: Buffered mode slave disable/enable)
; bit 1 (AUTO: normal/auto end of interrupt (EOI))
; bit 0 (8086: [MCS-80/85]/[8086-88] mode)

; In real mode IRQs 0-8 are mapped to interrupts 0x8 - 0xF 
; but in protected mode interrupts until 0x1F are reserved for CPU exceptions, so a re-mapping is needed
	mov al, 00010001b			; initialize, edge triggered mode, cascade mode, use ICW4 
	out 0x20, al				; IO base address for master PIC (command register): 0x20
	mov al, 00010001b			; initialize, edge triggered mode, cascade mode, use ICW4 
	out 0xa0, al				; IO base address for slave PIC (command register): 0xa0
        mov al, 0x20			        ; IRQ 0-7 mapped to interrupts 0x20-0x27
	out 0x21, al                            ; Master PIC data register IO address
	mov al, 0x28			        ; IRQ 8-15 mapped to interrupts 0x28-0x2F
	out 0xa1, al          			; Slave PIC data register IO address

	mov al, 00000100b			; ICW3: Slave mapped to IRQ 2		
	out 0x21, al                            ; Master PIC data register IO address
	mov al, 00000010b			; ICW3: assign second slave ID to slave PIC
	out 0xa1, al          			; Slave PIC data register IO address
	mov al, 1				; ICW4: 8086-88 mode
	out 0x21, al                            ; Master PIC data register IO address
	mov al, 1				; ICW4: 8086-88 mode
        out 0xa1, al          			; Slave PIC data register IO address
; Mask all PIC interrupts
	mov al, 11111111b
	out 0x21, al                            ; Master PIC data register IO address
	mov al, 11111111b
	out 0xa1, al          			; Slave PIC data register IO address

enterLongMode: 
        lgdt [GDT64DescriptorStruct]            ; load GDT descriptor struct
; enable Physical Address Extension (PAE)
        mov eax, cr4				; read control register 4 into eax
        or eax, (1 << 5)			; set 5th bit (PAE)
        mov cr4, eax				; enable PAE

; make control register 3 point to PML4T address
        mov eax, 0x70000			; PML4T address
        mov cr3, eax				; cr3 = PML4T address

; enable Long Mode
        mov ecx, 0xc0000080			; Extended Feature Enable Register (EFER) code for rdmsr
        rdmsr					; Read Model Specific Register (MSR) into eax
        or eax, (1 << 8)			; Set 8-th bit: LME (Long Mode Enable)
        wrmsr				        ; write eax to EFER to enable Long Mode

; enable paging
        mov eax, cr0				; read control register 0 into eax
        or eax, (1 << 31)			; set 31st bit: paging (PG)
        mov cr0, eax				; write eax to cr0 to enable paging

jumpKernel:
	jmp LONG_MODE_CODE_SEG:0x00200000 	; jump to Long Mode kernel entry

ataLBARead:
	mov ebx, eax				; backup eax
; send highest 8 bits of LBA address to ATA controller
	shr eax, 24				; highest 8-bits of LBA address
	or eax, 0xe0				; select the ATA master drive
        mov dx, 0x1f6				; IO Port for ATA controller
	out dx, al				; send highest 8-bits of LBA address
; send total number of sectors to read to ATA controller
	mov eax, ecx				; move number of sectors to read to al
        mov dx, 0x1f2				; IO port for ATA controller
	out dx, al				; send total number of sectors to read
; send lowest 8 bits of LBA address to ATA controller
	mov eax, ebx				; Load backed-up LBA address
	mov dx, 0x1f3				; IO port for ATA controller
	out dx, al				; send lowes 8 bits of LBA address
; send bits 15-8 of LBA address to ATA controller
	mov eax, ebx				; Load backed-up LBA address
        mov dx, 0x1f4				; IO port for ATA controller
        shr eax, 8				; bits 15-8 in al
        out dx, al				; send bits 15-8 of LBA address
; send bits 23-16 of LBA address to ATA controller
	mov eax, ebx				; Load backed-up LBA address
        mov dx, 0x1f5				; IO port for ATA controller
        shr eax, 16				; bits 23-16 in al
        out dx, al				; send bits 23-16 of LBA address

	mov dx, 0x1f7				; ATA controller IO control port
	mov al, 0x20				; READ SECTORS command
	out dx, al				; send READ SECTORS command to ATA IO control port
; read all sectors into memory
.nextSector:
	push ecx				; save total number of sectors to read onto the stack
; Poll ATA controller status port until ready: 4-th bit is set
.tryAgain:
	mov dx, 0x1f7				; ATA controller status port
	in al, dx				; read IO port
	test al, 0x08				; check if 4th bit set 
	jz .tryAgain				; jump back and try again if it is not
; read 256 half-words at a time
	mov ecx, 256				; number of half-words to be read
	mov dx, 0x1f0				; ATA controller data port
	rep insw				; repeat insw (read half-word from port) (counter in ecx)
	pop ecx					; pop number of sectors to be read from stack into ecx
	loop .nextSector			; loop decrements ecx and jumps to label if ecx != 0
	ret					; return from subroutine

;printString16:
;	push ax					; save ax on stack
;	push bx					; save bx on stack
;        xor bx, bx                              ; Set page number (bh) to 0 (bl is foreground color for graphics modes)
;        mov ah, 0x0e				; BIOS Video interrupt 0x10: Teletype Output
;.printLoop:  
;        lodsb                                   ; load mem[ds:si] in al and increment si 
;        or al, al				; set zero flag to 1, if al == 0
;        jz .cleanup                             ; jump to end if al == 0 (null string terminator) 
;        int 0x10				; BIOS Video interrupt 0x10
;        jmp .printLoop
;.cleanup:
;        pop ax					; restore saved value in ax
;        pop bx					; restore saved value in bx
;        ret					; return

; GDT definition
GDTStart:

GDTNullDesc:
	dd 0x0
	dd 0x0

GDTCodeSegDesc:					; CS should point to this descriptor
GDT_0x8_Desc:
	dw 0xffff                               ; bits 0-15 of segment limit
        dw 0x0                                  ; bits 0-15 of segment base
	db 0x0                                  ; bits 16-23 of segment base
                                               
        ; access byte
        ; 7 (present bit): 1, 6:5 (privilege ring): 0, 4 (type, system or code/data): 1,
        ; 3 (executable) : 1, 2 (direction for data seg, conforming for code seg): 0, 1 (R/W code/segment) : 1, 0 (accessed) : 0             
        db 10011000b                            
       
        db 11001111b                            ; 7 (granularity): 1 (4Kb page), 6 (size): 1 (32-bit), 5 (Long mode): 0, 4 (reserved) : 0, 3:0 - bits 16-19 of segment limit: 0xFF
        db 0                                    ; bits 24-31 of segment base

GDTDataSegDesc:                                 ; DS, SS, ES, FS, GS should point to this descriptor
GDT_0x16_Desc:      
	dw 0xffff                               ; bits 0-15 of segment limit
        dw 0x0                                  ; bits 0-15 of segment base
	db 0x0					; bits 16-23 of segment base
                                               
        ; access byte
	; 7 (present bit): 1, 6:5 (privilege ring): 0, 4 (type, system or code/data): 1,
	; 3 (executable) : 0, 2 (direction for data seg, conforming for code seg): 0, 1 (R/W code/segment) : 1, 0 (accessed) : 0             
        db 10010010b                            
       
        db 11001111b                            ; 7 (granularity): 1 (4Kb page), 6 (size): 1 (32-bit), 5 (Long mode): 0, 4 (reserved) : 0, 3:0 - bits 16-19 of segment limit: 0xFF
        db 0                                    ; bits 24-31 of segment base

GDTEnd:
GDTDescriptorStruct:
	dw GDTEnd - GDTStart - 1                ; size of GDT in bytes minus 1: max value is 65535, but the GDT can be up to 65536 bytes (8192 entries)
        dd GDTStart				; offset of GDT (32 bits or 64 bits in Long mode)
	

; LONG MODE (64-bit) GDT definition
GDT64Start:

GDT64NullDesc:
	dd 0x0
	dd 0x0

GDT64CodeSegDesc:				; CS should point to this descriptor
GDT64_0x8_Desc:
	dw 0xffff                               ; bits 0-15 of segment limit
        dw 0x0                                  ; bits 0-15 of segment base
	db 0x0                                  ; bits 16-23 of segment base
                                               
        ; access byte
        ; 7 (present bit): 1, 6:5 (privilege ring): 0, 4 (type, system or code/data): 1,
        ; 3 (executable) : 1, 2 (direction for data seg, conforming for code seg): 0, 1 (R/W code/data) : 1, 0 (accessed) : 0             
        db 10011000b                            
       
        db 10101111b                            ; 7 (granularity): 1 (4Kb page), 6 (descriptor size, 16-bit or 32-bit): 0 (for Long mode), 5 (Long mode): 1, 4 (reserved) : 0, 3:0 - bits 16-19 of segment limit: 0xFF
        db 0                                    ; bits 24-31 of segment base

GDT64DataSegDesc:                               ; DS, SS, ES, FS, GS should point to this descriptor
GDT64_0x10_Desc:      
	dw 0xffff                               ; bits 0-15 of segment limit
        dw 0x0                                  ; bits 0-15 of segment base
	db 0x0					; bits 16-23 of segment base
                                               
        ; access byte
	; 7 (present bit): 1, 6:5 (privilege ring): 0, 4 (type, system or code/data): 1,
	; 3 (executable) : 0, 2 (direction for data seg, conforming for code seg): 0, 1 (R/W code/data) : 1, 0 (accessed) : 0             
        db 10010010b                            
       
        db 10101111b                            ; 7 (granularity): 1 (4Kb page), 6 (descriptor size, 16-bit or 32-bit): 0 (for Long mode), 5 (Long mode): 1, 4 (reserved) : 0, 3:0 - bits 16-19 of segment limit: 0xFF
        db 0                                    ; bits 24-31 of segment base

GDT64End:
GDT64DescriptorStruct:
	dw GDT64End - GDT64Start - 1            ; size of GDT in bytes minus 1: max value is 65535, but the GDT can be up to 65536 bytes (8192 entries)
        dd GDT64Start				; offset of GDT (32 bits or 64 bits in Long mode)
FAT16ReservedSectors: dw 0x0000
;bootMsg: db 'Stage2!', 0xd, 0xa, 0 ; \r\n

times 512 - ($ - $$) db 0                       ; zero pad: 512 - (here - start of section) bytes 
