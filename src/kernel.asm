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


; NOTE: Multicore stack management
; Each core has an 8KB stack: (BP_STACK_POINTER - CORE_ID * 1024 * 8) down to (BP_STACK_POINTER - (CORE_ID + 1) * 1024 * 8)

%include "src/boot/defs.asm"

APmain equ 0x8000				; Application Processor (AP) startup vector address: 8 * 4KB
                                                ; This is the address of the code the AP starts executing after 
                                                ; the Start-up IPI is sent by BP

global getCoreId				; function that returns core id
global _start					; make _start label exportable

extern gActiveCpuCount 				; address of active CPU count variable
extern gLocalApicAddress 			; Local APIC address
extern kernelStart				; entry point function for C kernel
extern idtDesc                                  ; IDT descriptor
extern printk                                   ; print function
extern loadGDT 					; defined in gdt/gdt.c
extern localAPICInit                            ; defined in acpi/acpi.c
extern loadPageTable                            ; defined in memory/memory.c
extern loadIDTAP                                ; defined in idt/idt.c
extern startIdleProcess				; defined in process/process.c
extern enableSysCall				; defined in syscall/syscall.c

section .text
[BITS 64]

_start:                                  	
        mov rax, vaddr
        jmp rax		
; jmp to setup virtual address in rip
; __start is still identity mapped at 0x200000 at this point                
vaddr:          
        ; clean unused segment registers	
        xor rax, rax				
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax

        mov dh, byte[BPFlag]                    ; load BP flag
        cmp dh, 0x1                             ; compare flag in dh to 1
        jz short ap64 	        	        ; if flag is set, this is AP so jump
 
	; set 8KB stack for BP
        mov rbp, BP_STACK_POINTER             ; set base pointer at 2MB (kernel image start)
        mov rsp, rbp                            ; set stack pointer at 2MB

        mov byte[BPFlag], 0x1                   ; BP sets this value

	mov qword[APmain], 0x00000000007c00ea   ; write 16-bit "jmp 0x7c00" instruction (0x7c00ea) at AP startup vector address to have AP execute from start of bootloader code
        
        ; BP kernel initialization function
        call kernelStart			; call kernelStart function (C)
        jmp idleProcess				; jmp to idle process loop
ap64:
        ; clean unused segment registers
	xor rax, rax				
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax

        ; setup unique 1024-byte stack address for each AP
        mov rax, [qword gLocalApicAddress]      ; load Local APIC base address
        mov rsi, rax                             
        add rsi, LAPIC_ID_REG			; add Local APIC id register offset
	lodsd					; load register value into eax
        shr rax, 24				; id is stored in upped 8 bits, shift right 24
	mov rsi, rax                            ; save id in rsi for print function call below
        shl rax, 13				; multiply id by 8 * 1024 to get a 8KB unique stack address offset
        mov rbx, BP_STACK_POINTER 	        ; BP base stack pointer address
        sub rbx, rax                            ; subtract offset from previous core stack pointer address
        mov rbp, rbx		                ; set base pointer
        mov rsp, rbp		                ; set stack pointer
        ;;;;;; WARNING! The stack for each core is limited to 8KB at this point!!!!      
 
        ; load new GDT created by AP (gdt/gdt.c) 
        call loadGDT
 
        ; load Page Table created by AP (memory/memory.c) 
        call loadPageTable
       
        ; load IDT 
        call loadIDTAP				
 
        ; initialize Local APIC
        call localAPICInit					

        mov rdi, rsi
        ; increment active CPU/CORE count 
        mov rax, gActiveCpuCount
        lock					; lock cache line for inc instruction
        inc byte [rax]		                ; increment active core/cpu count       			 
 
        ; load task register (TSS)
        ; core ID was saved in rbx
        mov rax, [qword gLocalApicAddress]      ; load Local APIC base address
        mov rsi, rax                             
        add rsi, LAPIC_ID_REG			; add Local APIC id register offset
	lodsd					; load register value into eax
        shr rax, 24
        mov rsi, rax
        shl rax, 4				; multiply id by 16 (size of TSS descriptor) to get descriptor index for AP
        
        add rax, LONG_MODE_FIRST_TSS            
        ltr ax
        mov rdi, coreMsg
        xor rax, rax                            ; printk is a variadic function, rax=0 means no floating point arguments
        call printk
        call enableSysCall
        call startIdleProcess
idleProcess:               
        mov ax, LONG_MODE_DATA_SEG		; set ss to kernel mode code segment descriptor before enabling interrupts
	mov ss, ax
        sti					; enable interrupts
	hlt					; halt core until next interrupt
        jmp idleProcess
coreMsg: db 'Core %d started!', 0xa, 0 ; \r\n
getCoreId:					; function that returns core id
        mov rax, [qword gLocalApicAddress]      ; load Local APIC base address
        mov rsi, rax                             
        add rsi, LAPIC_ID_REG			; add Local APIC id register offset
	lodsd					; load register value into eax
        shr rax, 24				; id is stored in upped 8 bits, shift right 24
        retq 
times   512 - ($ - $$) db 0                     ; zero pad to 512-byte boundary: 512 - (here - start of section) bytes
						; this ensures C code put by the linker after this code is 16-byte aligned
