; Copyright 2023 Andrea Miele
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at

;   http://www.apache.org/licenses/LICENSE-2.0

; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

global loadCR3
global readCR3

section .text
; Long Mode
[BITS 64]

; load CR3 register (holding the page table physical address) with input value

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used

loadCR3:
        mov rax, rdi
        mov cr3, rax
        retq

; read CR3 register value (holding the page table physical address)

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used
; return value stored in rax
readCR3:
 	mov rax, cr3
        retq	

