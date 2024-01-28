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

global spinLock
global spinUnlock
global spinLockCli
global spinUnlockSti

section .text
; Long Mode
[BITS 64]

; x64 System V calling convention: parameters are passed in rdi, rsi, rdx, rcx, r8, r9; return value in rax
; and if there are more the stack is used

; void spinLockCli(uint8_t * lockAddress)
spinLockCli:
	cli			; disable interrupts
; void spinLock(uint8_t * lockAddress)
spinLock:
	mov bl, 1
.spin:
	xor rax, rax
	; atomic compare and exchange: if [rdi] (7 : 0) == al (0) then [rdi] (7 : 0) = bl = 1 and set zero flag 
        ; else al = [rdi] (7 : 0) and reset zero flag
	lock cmpxchg byte [rdi], bl
        jnz .spin		; keep trying
        ret
 
; void spinUnlock(uint8_t * lockAddress)
spinUnlock:
	xor rax, rax
 	; atomic exchange
        lock xchg byte [rdi], al ; [rdi] (7 : 0) = al = 0 and al = [rdi] (7 : 0)
	ret 
; void spinUnlockSti(uint8_t * lockAddress)
spinUnlockSti:
	xor rax, rax
	; atomic exchange	
	lock xchg byte [rdi], al ; [rdi] (7 : 0) = al = 0 and al = [rdi] (7 : 0)
	sti			 ; enable interrupts
	ret 
