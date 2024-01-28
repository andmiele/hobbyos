/*
 * Copyright 2024 Andrea Miele
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _IDT_H_
#define _IDT_H_

#include <stdint.h>

// x86-64 Interrupt Descriptor Table (IDT)

#define TOT_N_INTERRUPTS 256

// ist1 field will contain per-core ring0 stack
// for exceptions where we cannot rely on the per-process
// ring0 stack
#define POSSIBLE_CORRUPTED_RING0_STACK_IST 0x1
// Interrupt numbers of exceptions that need IST (we use ist1) per-core ring0
// stack instead of per-process ring0 stack

#define NON_MASKABLE_INTERRUPT 0x2
#define DOUBLE_FAULT 0x8
#define INVALID_TSS 0xA
#define STACK_SEGMENT_FAULT 0xC
#define GENERAL_PROTECTION_FAULT 0xD

#define TIMER_INTERRUPT 0x20
#define KEYBOARD_INTERRUPT 0x21
#define SPURIOUS_INTERRUPT 0xFF

// PS2 KEYBOARD
// NOTE: move to keyboard driver files eventually
#define PS2_DATA_IO_PORT 0x60
#define PS2_COMMAND_IO_PORT 0x64
#define PS2_ENABLE_FIRST_PORT_CMD 0xAE
#define PS2_DISABLE_FIRST_PORT_CMD 0xAD
#define PS2_ENABLE_SECOND_PORT_CMD 0xA8
#define PS2_DISABLE_SECOND_PORT_CMD 0xA7
#define PS2_RESET_CMD 0xFF
#define PS2_SELF_TEST_CMD 0xAA
#define PS2_WRITE_NEXT_BYTE_0_CMD 0x60
#define PS2_READ_BYTE_0_CMD 0x20

#define PS2_OUTPUT_FULL 0x1

// interrupt frame for interrupt handler
// size: 184 bytes
struct interruptFrame {
  int64_t r15;
  int64_t r14;
  int64_t r13;
  int64_t r12;
  int64_t r11;
  int64_t r10;
  int64_t r9;
  int64_t r8;
  int64_t rbp;
  int64_t rdi;
  int64_t rsi;
  int64_t rdx;
  int64_t rcx;
  int64_t rbx;
  int64_t rax;
  int64_t coreId;
  int64_t intNumber;
  int64_t errorCode;
  // automatically pushed by CPU:
  int64_t rip;
  int64_t cs;
  int64_t rflags;
  int64_t rsp;
  int64_t ss;
} __attribute__((packed));

struct idtEntryDescriptor {
  uint16_t offsetLow;  // offset bits 0-15
  uint16_t selector;   // a segment selector pointing to a valid code segment
                       // defined in the GDT

  // Interrupt Stack Table (IST) offset, bits 0-2, (offset into the Interrupt
  // Stack Table stored in the Task State Segment(TSS). If zero, the Interrupt
  // Stack Table is not used) Reserved, bits 3-7, not currently used, set to
  // zero
  uint8_t ISTAndReserved;

  // Gate type, bits 0-3 (0b1110 or 0xE: 64-bit Interrupt Gate, 0b1111 or 0xF:
  // 64-bit Trap Gate) zero: bit 4 (set to zero) DPL, bits 5-6 (defines the CPU
  // ring levels that are allowed to call this interrupt via the int
  // instruction. Hardware interrupts ignore this mechanism)
  // P, bit 7 (Present bit; must be set (1) for the descriptor to be valid
  uint8_t attributes;

  uint16_t offsetMid;   // offset bits 16-31
  uint32_t offsetHigh;  // offset bits 32-63
  uint32_t reserved;    // not currently used, set to zero
} __attribute__((packed));

struct idtDescriptor {
  uint16_t size;     // size of IDT minus 1
  uint64_t address;  // The virtual address of the IDT
} __attribute__((packed));

// initialize Interrupt Desctiptor Table
void initializeIDT();

// return timer interrupt tick count
uint64_t getTicks();
#endif
