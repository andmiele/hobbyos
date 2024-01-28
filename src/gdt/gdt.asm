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

global loadGDTAndCS
global loadTaskRegister

section .text
; Long Mode
[BITS 64]

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9
; and if there are more the stack is used

                  ; rdi                   ; rsi
; void loadGDTAndCS(void* gdtDescPtr, uint64_t gdtDescIndex)
loadGDTAndCS:
	lgdt [rdi]        ; load GDT descriptor
        push rsi          ; push gdtDescriptorIndex; this should be a code segment descriptor
        mov  rax, return  ; push address of return label
        push rax
        jmp far [rsp]     ; jmp gtdDescIndex:return to set CS to gdtDescIndex
return:
        ret
; load task register with TSS descriptor selector
loadTaskRegister:
	mov rax, rdi
        ltr ax
        ret
