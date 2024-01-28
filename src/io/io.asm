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

section .text

global inb
global inh
global inw
global outb
global outh
global outw

; Long Mode
[BITS 64]

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9; return value in rax
; and if there are more the stack is used

inb:
	xor rax, rax	; zero out rax
	mov rdx, rdi	; move first argument (16-bit port number) into rdx (dx)
	in al, dx	; IO byte read instruction, read byte into al (return value)
	retq
inh:
	xor rax, rax	; zero out rax
	mov rdx, rdi	; move first argument (16-bit port number) into rdx (dx)
	in ax, dx	; IO half-word read instruction, read half-word into ax (return value)
	retq
inw:
	xor rax, rax	; zero out rax
	mov rdx, rdi	; move first argument (16-bit port number) into rdx (dx)
	in eax, dx	; IO word read instruction, read word into eax (return value)
	retq
outb:
	mov rdx, rdi	; move first argument (16-bit port number) into rdx (dx)
	mov rax, rsi	; move second argument (8-bit value) into rax (al)
	out dx, al	; IO byte write instruction, write byte (al) to IO port (dx)
	retq
outh:
	mov rdx, rdi	; move first argument (16-bit port number) into rdx (dx)
	mov rax, rsi	; move second argument (8-bit value) into rax (al)
	out dx, ax	; IO half-word write instruction, write half-word (ax) to IO port (dx)
	retq
outw:
	mov rdx, rdi	; move first argument (16-bit port number) into rdx (dx)
	mov rax, rsi	; move second argument (8-bit value) into rax (al)
	out dx, eax	; IO word write instruction, write word (eax) to IO port (dx)
	retq
