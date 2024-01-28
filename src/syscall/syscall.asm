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

extern gLocalApicAddress 
extern ring0SysCallStackPtrTable
extern systemCallTable

extern syscallRunningArray
extern syscallRunSchedulerArray

extern yield
extern returnFromTimerInterrupt 
global enableSysCall

IA32_LSTAR_MSR  equ 0xC0000082	; Model-specific register (MSR): holds the system call entry point address
IA32_STAR_MSR   equ 0xC0000081	; Model-specific register (MSR): holds ring0 and ring3 code segment selectos 
IA32_FMASK_MSR  equ 0xC0000084	; Model-specific register (MSR): holds mask for RFLAGS applied upon syscall entry 
IA32_EFER_MSR   equ 0xC0000080  ; Model-specific register (MSR): Extended Feature Enable Register (EFER)   

section .text
; Long Mode
[BITS 64]


; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used

enableSysCall:
	mov rcx, IA32_LSTAR_MSR 	; destination MSR code
	; rdmsr
	mov rax, syscallEntryPoint  	; System call entry point function
        ; write edx:eax to MSR; high 32-bits of rax and rdx are ignored
        ; so high 32 bit of System call entry point address are in rdx and low 32 bits in eax
        mov rdx, rax
	shr rdx, 32
	wrmsr
	; Set-up code segments for syscall/sysexit
	mov rcx, IA32_STAR_MSR		; destination MSR code
	xor rax, rax			; zero out rax
	; ring0 code segment at offset 0 (stack segment selector is assumed to be next selector)
	; ring3 code segment at offset 16 (stack segment selector is assumed to be next selector)
        ; make sure GDT is set-up accordingly
	mov rdx, LONG_MODE_CODE_SEG | (((LONG_MODE_DATA_SEG + 8) | DPL_RING3) << 16)
	wrmsr
        ; uncomment to disable interrupts upon sycall entry (write RFLAGS mask to IA32_FMASK)
        ; we have a per-process  syscall ring0 stack so we can serve interrupts during a syscall (TSS)
        xor rdx, rdx			; zero out rdx
        mov rax, 0x200			; RFLAGS disable interrupt bit set in mask
        mov rcx, IA32_FMASK_MSR         ; destination MSR code
        wrmsr 
	; Enable syscall by setting bit 0 on EFER MSR
	mov ecx, IA32_EFER_MSR      	; destination MSR code
	rdmsr
	bts eax, 0                  	; test and set: SYSCALL enable bit = 1
	wrmsr
	retq

syscallEntryPoint:
        ; rip saved in rcx, RFLAGS saved in r11
        ; retrieve per process ring0 4KB stack 
        mov r15, rsi 			; save rsi
        ; get core id
        mov rax, [qword gLocalApicAddress]
	mov rsi, rax                             
        add rsi, LAPIC_ID_REG		; add Local APIC id register offset
	lodsd				; load register value into eax
        shr rax, 24			; id is stored in upped 8 bits, shift right 24
       
        shl rax, 3			; multiply core id by 8 to get offset
       
        mov r13, rsp			; save userspace rsp
        
        ; set rsp to per-process ring0 stack top 
        mov rbx, ring0SysCallStackPtrTable
        add rbx, rax
        mov r14, [rbx]
        mov rsp, r14
        
        sub rsp, 184			; subtract interruptFrame size: 184
 
        cmp rdi, 11			; exec syscall index
        je execSyscallAdjustReturnParameters
        push r13			; push saved userspace rsp
	push rbp
	push rcx			; saved rip
	push r11			; saved RFLAGS
        jmp checkSyscallIndex		
execSyscallAdjustReturnParameters:	; adjust rsp, rbp, rip and RFLAGS for exec syscall
	push DEFAULT_USER_RSP 		; userspace rsp
        push rbp
        push USER_PROGRAM_COUNTER	; rip
        push r11			; saved RFLAGS         
checkSyscallIndex:
	cmp rdi, N_SYSCALLS		; make sure syscall number is not larger than number of supported system calls
	jge .epilogue

        mov rbx, syscallRunningArray
        add rbx, rax			; add (core id) * 8 to get syscallRunningArray offset
        inc rdi
        mov qword[rbx], rdi
        dec rdi	
 
	; stack is set up; syscallRunning flag is set
        ; enable interrupts
.sti:
        sti

        shl rdi, 3			; multiply syscall number by 8 to find systemCallTable offset 
	mov rax, systemCallTable
        add rax, rdi
        
        ; Shift arguments (rdi, rsi, rdx, rcx, r8); At the moment each system call can have at most 5 parameters
        ; rdi contained syscall number argument 
        ; rcx is overwritten by syscall with rip, so rcx syscall argument is stored in r10 instead
        ; see src/userspace/syscall.asm
       
        ; if fork syscall pass special arguments: r13 (saved userspace rsp), rbp,  rcx (saved rip), r11 (saved RFLAGS)
        shr rdi, 3  
        cmp rdi, 10			; fork syscall index
	je .forkSysCallArgs		
        mov rdi, r15			; rsi was stored in r15 temporarily
        mov rsi, rdx
        mov rdx, r10			; rcx argument is stored in r10 as rcx is overwritten with rip by syscall
        mov rcx, r8 			
        mov r8, r9
        jmp .callFunction
.forkSysCallArgs:
        mov rdi, r13 			; saved userspace rsp
        mov rsi, rbp                    ; rbp
        mov rdx, rcx			; saved rip
	mov rcx, r11			; saved rflags
.callFunction:        
 	; disable interrupts as some handlers acquire locks and/or run the scheduler
        ; this should be done on an ISR critical section basis ideally to be optimal
        cli
	call [rax]			; call system call function
.epilogue:
        ; disable interrupts
        cli
        push rax			; save return address value for non void syscalls
        ; get core id
        mov rax, [qword gLocalApicAddress]
	mov rsi, rax                             
        add rsi, LAPIC_ID_REG		; add Local APIC id register offset
	lodsd				; load register value into eax
        shr rax, 24			; id is stored in upped 8 bits, shift right 24
        shl rax, 3			; multiply core id by 8 to get offset
        ; set current core's syscallRunning flag to 0 to notify ISRs that
        ; syscall is not in progress if interrupt occurs 
        mov rbx, syscallRunningArray
        add rbx, rax			; add (core id) * 8 offset to syscallRunningArray
        mov qword[rbx], 0	

        ; check if syscall was interrupted by timer interrupt and
	; scheduler needs to be invoked to switch process
        mov rbx, syscallRunSchedulerArray
        add rbx, rax			; add (core id) * 8 offset to syscallRunSchedulerArray
        mov r14, [rbx] 	
        cmp r14, 0
	je .return			
        mov qword[rbx], 0		; reset syscallRunSchedulerArray flag
 
        ;mov rdi, r13			; rip
	;mov rsi, r12			; RFLAGS
        call yield 			; if syscall was interrupted by timer interrupt, switch process
        ; process will restart here in ring0 next time it is selected by scheduler run by the timer interrupt handler
        ; so signal end of (timer) interrupt (EOI) to LAPIC 
        mov rax, [qword gLocalApicAddress]
        mov edi, eax
        add edi, LOCAL_APIC_EOI_REG
        xor eax, eax
        mov DWORD [edi], eax	
.return:
        pop rax				; restore return value for non void syscalls
        pop r11				; RFLAGS
	pop rcx				; rip
        pop rbp
	pop rsp        
        o64 sysret
