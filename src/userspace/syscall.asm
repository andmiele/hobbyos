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

global sysCall
global sleep
global exit
global pwait
global readCharFromkeyboard
global getMemorySize
global openFile
global readFile
global closeFile
global getFileSize
global fork
global exec
global getRootDirEntries

section .asm
; Long Mode
[BITS 64]

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used

; rdi is syscall number and rsi, rdx, rcx, r8, r9 contain system call parameters
; rcx is overwritten with rip by CPU so save in r10
sysCall:
        ; Save 6 x64 callee-save registers on stack
	push rbx
    	push rbp
    	push r12
    	push r13
    	push r14
    	push r15
 
        mov r10, rcx
	syscall				; x64 syscall instruction
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbp
        pop rbx
        ret

sleep:
	mov rsi, rdi			; nTicks
        mov rdi, 1			; sysSleep syscall index
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
exit:
        mov rdi, 2			; sysExit syscall index
        mov rsi, 0
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
pwait:
        mov rsi, rdi			; process pid
        mov rdi, 3			; sysWait syscall index
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
readCharFromkeyboard:
        mov rdi, 4			; sysReadCharFromKeyboardQueue syscall index
        mov rsi, 0
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
getMemorySize:
        mov rdi, 5			; sysGetMemorySize syscall index
        mov rsi, 0
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
openFile:
        mov rsi, rdi			; file name
        mov rdi, 6			; openFile syscall index
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
readFile:
        mov rcx, rdx			; size
        mov rdx, rsi			; fileBuffer pointer
        mov rsi, rdi			; file descriptor index
        mov rdi, 7			; readFile syscall index
        mov r8, 0
	mov r9, 0
        jmp sysCall
closeFile:
        mov rsi, rdi			; file descriptor index
        mov rdi, 8			; closeFile syscall index
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
getFileSize:
        mov rsi, rdi			; file descriptor index
        mov rdi, 9			; getFileSize syscall index
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
fork:
        mov rdi, 10			; fork syscall index
        mov rsi, 0
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
exec:
        mov rsi, rdi			; file name
        mov rdi, 11			; exec syscall index
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
getRootDirEntries:
        mov rsi, rdi			; input buffer
        mov rdi, 12			; getRootDirEntries syscall index
        mov rdx, 0
	mov rcx, 0
        mov r8, 0
	mov r9, 0
        jmp sysCall
