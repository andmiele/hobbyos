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

; assembly code definitions

MAX_N_CORES equ 64
N_KERNEL_DISK_SECTORS equ 217
N_USERSPACE_DISK_SECTORS equ 9
N_USER_PROCESSES equ 1

BPFlag equ 0x7dff				; address of Bootstrap Processor (BP) flag; 
                                                ; set flag after boot time setup operations are complete so that Application Processors (APs) 
                                                ; can execute starting from the real-mode first stage bootloader but skip (using conditional jumps depending on the BPFlag)
                                                ; some parts of the first and second stage bootloader code that should only be run by the BP.
                                                ; Use last byte of bootsector copy in memory where last byte of 
                                                ; bootsector magic number is expected to be stored (0xaa) 

gMemoryMap equ 0x6018 				; address at which the E820 Memory Map is stored once retrieved 
gNMemoryRegions equ 0x6010 			; address at which the E820 Memory Region count is stored

LOCAL_APIC_EOI_REG EQU 0xb0			; Local APIC end-of-interrupt command
LAPIC_ID_REG equ 0x20			        ; Local APIC id register offset
LAPIC_SPURIOUS_INT_VEC_REG equ 0xF0             ; Local APIC spurious interrupt register offset

LONG_MODE_CODE_SEG equ 0x08			; first 8-byte GDT descriptor after null one
LONG_MODE_DATA_SEG equ 0x10			; second 8-byte GDT descriptor after null one
LONG_MODE_USER_DATA_SEG equ 0x20		; fourth 8-byte GDT descriptor after null one
LONG_MODE_USER_CODE_SEG equ 0x28		; fifth 8-byte GDT descriptor after null one
LONG_MODE_FIRST_TSS equ 0x30		        ; sixth 8-byte GDT descriptor after null one
DPL_RING3 equ 0x3			        ; ring3: 3

N_SYSCALLS equ 13				; number of supported system calls

BP_STACK_POINTER equ 0xffff800000200000         ; higher ring0 stack pointer address: Bootstrap processor ring0 stack pointer

USER_PROGRAM_COUNTER equ 0x400000               ; virtual address of starting user program counter
DEFAULT_TOTAL_PROCESS_SIZE equ (1024 * 64)       ; user process space size
DEFAULT_USER_RSP equ (USER_PROGRAM_COUNTER + DEFAULT_TOTAL_PROCESS_SIZE) ; user process default stack pointer
