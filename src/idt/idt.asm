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

%include "src/boot/defs.asm"

; process yield after timer interrupt
extern yield
extern spinUnlock
extern processLock 
global startUserProcess
global switchUserProcess
global returnFromTimerInterrupt
; First 32 interrupts are exceptions (e.g., 14 is Page Fault)
; CPU pushes rip, cs, rflags, rsp and ss onto the stack before calling the isr
; For some exceptions (like Page Fault), it also pushes an error code 

%macro saveRegisters  0
	push rax
	push rbx  
	push rcx
	push rdx  	  
	push rsi
	push rdi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
%endmacro 

%macro restoreRegisters 0
	pop r15
	pop r14  
	pop r13
	pop r12  	  
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	pop rax
%endmacro 

; prologue: push interruptFrame struct to stack: registers and other fields (see idt/idt.h), read CPU core id and store it in rax
%macro prologue 1
        ; for some exceptions, like Page Fault (int14), the CPU pushes an error code before rip  
        ; push 0 for all the others
        %if %1 < 8
          push 0                                ; push errorCode 0 to stack (interruptFrame struct)
        %elif %1 == 9
          push 0                                ; push errorCode 0 to stack (interruptFrame struct)
        %elif %1 >= 15 && %1 <= 16
          push 0                                ; push errorCode 0 to stack (interruptFrame struct)
        %elif %1 >= 18 && %1 <= 20
          push 0                                ; push errorCode 0 to stack (interruptFrame struct)
        %elif %1 >= 22 && %1 <= 28
          push 0                                ; push errorCode 0 to stack (interruptFrame struct)
        %elif %1 >= 31
          push 0                                ; push errorCode 0 to stack (interruptFrame struct)
        %endif
        push %1					; push intNumber (interrupt / isr number) tos tack (interruptFrame struct)

        ; get core id
        push 0					; placholder for core id; need to user space preserve rax, rsi
        push rax
        push rsi
        mov rax, [qword gLocalApicAddress]
	mov rsi, rax                             
        add rsi, LAPIC_ID_REG			; add Local APIC id register offset
	lodsd					; load register value into eax
        shr rax, 24				; id is stored in upped 8 bits, shift right 24
        pop rsi					; restore rsi
        ; rsp points to saved rax at this point
        add rsp, 16				; add 8 to rsp to point to 64-bit value before placeholder
        push rax				; push coreId to stack (interruptFrame struct); overwrite placeholder
        sub rsp, 8				; rsp points to saved rax now
        pop rax					; restore rax
        
        saveRegisters                           ; push registers to stack (interruptFrame struct)
%endmacro

; epilogue: acknowledge interrupt, restore registers and stack pointer
%macro epilogue 0
        mov rax, [qword gLocalApicAddress]
        mov edi, eax
        add edi, LOCAL_APIC_EOI_REG
        xor eax, eax
        mov DWORD [edi], eax
        restoreRegisters
        add rsp, 24  				; intNumber, errorCode, coreId
%endmacro

; Use macro to initialize all the 512 interrupt service routines (ISRs)
; using the generic interrupt handler function that takes the ISR  address
; as input parameter (rdi)
%macro interruptServiceRoutine 1 ; one input: interrupt number
global isr%1 ; make isr1, isr2, ... labels global
isr%1: 
        prologue %1
        ; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
        ; and if there are more the stack is used
        mov rdi, rsp                            ; pass interruptFrame struct pointer
        call selectInterruptHandler
        %if %1 == 32				; timer interrupt
returnFromTimerInterrupt:
        %endif
        epilogue
        iretq
%endmacro

extern gLocalApicAddress 
extern int20Handler
extern int21Handler
extern selectInterruptHandler

global isrAddressArray
global int20
global int21
global intFF
global noInt
global loadIDT
global readCR2

section .text
; Long Mode
[BITS 64]

; Create 256 ISRs
%assign i 0
%rep 256
        interruptServiceRoutine i
%assign i i+1
%endrep 

isrAddressArray:
; Create array of pointers/addresses to each of the 256 ISRs

%macro isrAddr 1
        dq isr%1
%endmacro

%assign i 0
%rep 256
        isrAddr i
%assign i i+1
%endrep 



; switch user process

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used
; return value stored in rax

; Save 6 x64 callee-save registers on stack
; rsp must point current process' ring0ProcessContext before calling switchUserProcess
switchUserProcess:
        push rbx
    	push rbp
    	push r12
    	push r13
    	push r14
    	push r15
    	mov [rdi], rsp	; first argument is address of saved user process ring0ProcessContext
    	mov rsp, rsi	; second argument is stack pointer (ring0ProcessContext address) for process we switch to
        ; restore 6 x64 callee-save registers
    	pop r15
    	pop r14
    	pop r13
    	pop r12
    	pop rbp
    	pop rbx
   
        ; if we are inside timer ISR 
        ; at this point esp points to ring0ProcessContext->ret,
	; which is address of returnFromTimerInterrupt function
        mov rdi, processLock     
        call spinUnlock
        ret	


; enter ring3 / start user process

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used
; return value stored in rax

startUserProcess:
        mov rsp, rdi	; interrupt frame pointer passed as first argument
        restoreRegisters
	add rsp, 24  	; intNumber, errorCode, coreId
        iretq

; read CR2 register containing virtual address that caused exception

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used
; return value stored in rax
readCR2:
	mov rax, cr2
        retq	

; load IDT

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used
loadIDT:
	lidt [rdi]
	retq

; Spurious interrupt
intFF:
	iretq
noInt:
        prologue
        mov rax, 0xffff8000000b8000
        mov byte [rax], "N"
	epilogue
        iretq
