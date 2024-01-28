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

; First Stage Bootloader

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
; 512-byte loader/second stage (second sector) copied from disk to:
; 0x07e00 - 0x08000

%include "src/boot/defs.asm"
BITS 16                                         ; tell assembler we are in 16-bit mode
ORG 0x7c00                                      ; address at which BIOS loads the bootloader is 0x7c00


; BPB (BIOS Parameter Block)
_BPB_start:                                     ; BIOS Parameter Block (BPB) start label; 
                                                ; some BIOS implementations fill some of the fields of the BPB (33 bytes overall)
        jmp short main                          ; first BPB entry is code: jmp short 3C (3C can be replaced by any offset)
        nop                                     ; second BPB entry is code: nop     
                                 
FAT16: 						; FAT 16 header: 30 bytes
OEMIdentifier db 'HOBBY_OS'                     ; OS identifier
BytesPerSector dw 512				; sector is 512 bytes; often ignored 
SectorsPerCluster db 128			; sectors per cluster: 128 (65536 bytes) is the max FAT16 value
ReservedSectors dw N_KERNEL_DISK_SECTORS + 2    ; number of reserved sectors: currently number of sectors the kernel occupies on disk plus 2 (boot sector and loader)
FATCopies db 2					; we have two copies of the FAT table
NumOfRootDirectoryEntries dw 256		; number of entries in the root directory
SmallNumSectors dw 0				; small number of sectors, used if there are at most 65536 sectors
MediaType db 0xf8				; hard disk type
SectorsPerFAT dw 256				; number of sectors per FAT
SectorsPerTrack dw 32				; number of sector per hard disk track
NumberOfHeads dw 64				; number of heads
HiddenSectors dd 0				; number of hidden sectors
SectorsBig dd 0x773594		        	; large sector count, used if there are are more than 65536 sectors

; Extended FAT16 header, 26 bytes				
DriveNumber db 0x80				; drive number: 0x00 for floppy disk, 0x80 for hard disk
WinNTBit db 0					; reserved
Signature db 0x28				; signature: must be 0x28 or 0x29 to indicate that the next 3 fields are valid
VolumeID dd 0xd105				; Parition serial number
VolumeIDString db 'HOBBYOS_0.1'		        ; Partition label string: 11 bytes
SystemIDString db 'FAT16   '			; File System identifier string: 8 bytes

main:
        jmp 0x0:main2                           ; set CS to 0x0

main2:
        mov byte[driveId], dl			; save drive id (automatically stored in dl at boot)
 	cli					; disable interrupts as we change segment registers
        mov ax, 0x0				; set ax to 0x0 value to be used for segment registers
        mov ds, ax                              ; ds = 0x0
        mov es, ax                              ; es = 0x0
        mov ss, ax				; set stack segment to 0; the stack is 0x7c00 : 0x0000
	mov sp, 0x7c00				; set stack pointer to bootloader start memory address

; SMP: bootstrap processor (BP) sets flag in memory so that application processors (AP can skip some parts of the initialization code
        mov dh, byte[BPFlag]                    ; load BP flag
        cmp dh, 0x01                            ; compare flag stored in dh to 1
        jz 0x7e00 	                        ; if flag is set, this is AP so jump to second stage (loader.asm)

; Set Video Mode, int 0x10, ah=0x00
; 0x03: 80x25 16 color text (CGA,EGA,MCGA,VGA)
; 0x10: graphics, 640 x 350 , EGA, 16 colors, address 0xA0000 

        mov al, 0x03				; Video Mode
	int 0x10

; check if BIOS supports 0x13 extensions and LBA mode
        mov ah, 0x41                            ; int 0x13 command to check for extensions
        mov bx, 0x55aa				; code for checking LBA support
        int 0x13
        mov si, int13ExtensionsErrorMsg         ; load address of int 0x13 "extensions not supported" error message in si
        jc error                                ; carry flag set if disk extensions are not supported
        cmp bx, 0xaa55				; if bx != 0xaa55 LBA is not supported
        mov si, int13LBAErrorMsg                ; load address of int 0x13 "LBA not supported" error message in si
        jne error                        
        
        ; read loader/second stage from disk (second sector) with int 0x13
        mov ah, 2                               ; ah = read sector command
        mov al, 1                               ; al = number of sectors to read
        mov ch, 0                               ; ch = low 8 bits of cylinder number
        mov cl, 2                               ; cl = sector number (bits 0-5), high bits of cylinder number (bits 6-7)
        mov dh, 0                               ; dh = head number    
        mov bx, 0x7e00                          ; es:bx = data buffer
        mov dl, byte[driveId]
        int 0x13                                ; interrupt 13                      
        mov si, diskReadErrorMsg                ; load disk read error message in si
        jc error                                ; interrupt 13: error if carry flag is set

; Check for long mode and 1GB page support
        mov eax, 0x80000000			; check if extended functions of CPUID are supported
        cpuid
        cmp eax, 0x80000001
        mov si, cpuidExtendedFunctionsErrorMsg  ; load address of cpuid "extended functions not supported" error message in si
        jb error                                ; if less than 0x80000001 not supported
        mov eax, 0x80000001
        cpuid
        test edx, 0x20000000                    ; check if Long Mode is supported
        mov si, cpuidLongModeErrorMsg           ; load address of cpuid "Long Mode not supported" error message in si
        jz error
        test edx, 0x04000000                    ; check if 1GB pages are supported
        mov si, cpuid1GBPagesErrorMsg           ; load address of cpuid "1GB pages not supported" error message in si
        jz error 
 
            
; Get E820 Memory Map
        mov dword[gNMemoryRegions], 0           ; set memory region count to 0
        mov di, gMemoryMap                      ; load address at which we will store memory map 
        xor ebx, ebx                            ; set int 0x15 state to 0
.eb20Loop:
        mov eax, 0xe820                         ; call int 0x15 with 0xe820 command (memory map)
        mov edx, 0x534d4150                     ; set edx to magic number
        mov ecx, 24                             ; set ecx to 24
        int 0x15                                ; call interrupt

        jc .e820Done                            ; carry flag set: not supported or end of list

        cmp eax, 0x534d4150                     ; eax value should match magic number
        jne .e820Done

        jcxz .next_entry                        ; skip zero-length entries (jump if ecx is zero)

        cmp cl, 20                              ; number of bytes written to es:di is stored in cl 
        jbe .valid_entry                        ; jump if below or equal to 20 bytes were written
                                                ; otherwise bytes 20-23 are ACPI 3.X extended attributes (24 bytes written in total)
        test byte [es:di + 20], 1               ; test ACPI 3.X ignore bit (first extended attribute bit)
        je .next_entry

.valid_entry:
        inc dword[gNMemoryRegions]              ; increment memory region count by 1
        add di, 24                              ; update di

.next_entry:
        test ebx, ebx                           ; ebx is zero if there are not more entries
        jne .eb20Loop

.e820Done:
        xor ax, ax                              ; Write terminating entry
        mov cx, 12
        rep stosw

        mov si, bootMsg		                ; put bootMsg string address in si
        call printString16			; call 16-bit string print function

        mov dl, byte[driveId]                   ; save drive id in dl
        mov cx, word[ReservedSectors]           ; move number of FAT16 reserved sectors to cx; used by loader to load kernel from disk
jumpToSecondStage:
       jmp 0x7e00				; jump to second stage / loader

; Real Mode error handling
error:
        call printString16
.error:
	jmp .error

printString16:
	push ax					; save ax on stack
	push bx					; save bx on stack
        xor bx, bx                              ; Set page number (bh) to 0 (bl is foreground color for graphics modes)
        mov ah, 0x0e				; BIOS Video interrupt 0x10: Teletype Output
.printLoop:  
        lodsb                                   ; load mem[ds:si] in al and increment si 
        or al, al				; set zero flag to 1, if al == 0
        jz .cleanup                             ; jump to end if al == 0 (null string terminator) 
        int 0x10				; BIOS Video interrupt 0x10
        jmp .printLoop
.cleanup:
        pop ax					; restore saved value in ax
        pop bx					; restore saved value in bx
        ret					; return

diskReadErrorMsg: db 'ERR: int 0x13 disk read', 0
int13ExtensionsErrorMsg: db 'ERR: no disk extensions', 0
int13LBAErrorMsg: db 'ERR: No LBA support', 0
cpuidExtendedFunctionsErrorMsg: db 'ERR: no cpuid extended funcs', 0
cpuidLongModeErrorMsg: db 'ERR: No Long Mode support', 0
cpuid1GBPagesErrorMsg: db 'ERR: No 1GB page support', 0
bootMsg: db 'Stage1!', 0xd, 0xa, 0 ; \r\n
driveId: db 0x00        

; PARTITION TABLE normally starts at 0x1be and contains 4 entries
; First partition table entry of MBR structure, needed by BIOS on old systems to make sure the USB stick is treated as a hard disk and not a floppy disk 
;times (0x1be-($-$$)) db 0

;	db 0x80					; 0x80 active/bootable partition (0x0 for not-active)
;       db 0x0                                  ; starting head
;        db 0x2                                  ; bits 0-5: starting sector (from 1); bits 6-7 are bits 8-9 of starting cylinder
;        db 0x0                                  ; bits 0-7 of starting cylinder
;        db 0x0f                                 ; System ID: WIN95 Extended partition, LBA-mapped
;        db 0xff                                 ; last head
;        db 0xff					; last sector
;	db 0xff					; Last cylinder
;        dd 0x00000001                           ; LBA address of first partition sector
;        dd 20480                                ; size in sectors of the partition; 10MB = 20480 * 512

times 510 - ($ - $$) db 0                       ; zero pad: 510 - (here - start of section) bytes 
dw 0xaa55                                       ; magic number for BIOS to identfy MBR, little endian, 2 bytes 511-512
