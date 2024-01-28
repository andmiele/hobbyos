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

#ifndef _GDT_H_
#define _GDT_H_

#include <stdint.h>

#define N_GDT_SEGMENT_DESCRIPTORS 6
#define GDT_SIZE                                              \
  (N_GDT_SEGMENT_DESCRIPTORS * sizeof(struct gdtDescriptor) + \
   MAX_N_CORES_SUPPORTED * sizeof(struct tssDescriptor))

#define RING3_SELECTOR_BITS 0x3
#define CODE_SEG_SELECTOR 0x08
#define DATA_SEG_SELECTOR 0x10
#define USER_DATA_SEG_SELECTOR 0x20
#define USER_CODE_SEG_SELECTOR 0x28

#define LONG_MODE_FIRST_TSS 0x30  // sixth 8-byte GDT descriptor after null one

#define GDT_DESC_ACCESS_BYTE_PRESENT 0x80
#define GDT_DESC_ACCESS_BYTE_DPL_RING3 0x60
#define GDT_DESC_ACCESS_BYTE_CODE_DATA_TYPE 0x10
#define GDT_DESC_ACCESS_BYTE_EXECUTABLE 0x8
#define GDT_DESC_ACCESS_BYTE_SEGMENT_GROWS_DOWN 0x4
#define GDT_DESC_ACCESS_BYTE_CONFORMING 0x4
#define GDT_DESC_ACCESS_BYTE_DATA_SEGMENT_WRITABLE 0x2
#define GDT_DESC_ACCESS_BYTE_CODE_SEGMENT_READABLE 0x2
#define GDT_DESC_ACCESS_BYTE_GDT_DESC_ACCESSED 0x1

#define GDT_DESC_FLAGS_4KB_GRANULARITY 0x8
#define GDT_DESC_FLAGS_32_BIT_DATA_SEGMENT_DESCRIPTOR 0x4
#define GDT_DESC_FLAGS_LONG_MODE_CODE_DESCRIPTOR 0x2

#define TSS_DESC_TYPE_LDT 0x2
#define TSS_DESC_TYPE_TSS_AVAILABLE 0x9
#define TSS_DESC_TYPE_TSS_BUSY 0xB

#define TSS_DESC_ACCESS_NIBBLE_PRESENT 0x8
#define TSS_DESC_ACCESS_NIBBLE_DPL_RING3 0x6

// Base and limit are ignored in Long Mode
struct gdtDescriptor {
  uint16_t segmentLimitBits0_15;  // bits 0-15 of segment limit
  uint16_t segmentBaseBits0_15;   // bits 0-15 of segment base
  uint8_t segmentBaseBits16_23;   // bits 16-23 of segment base

  // access byte
  // 7: present bit, 6-5: DPL, descriptor privilege level / ring level,
  // 4: type, system or code/data, 3: executable, 2: direction for data
  // seg, 0: grows up, 1: grows  down and conforming bit for code seg: 0 can
  // only be jumped to from code with same DPL, 1 (conforming): can be jumped to
  // from code with higher DPL, 1: R/W for data segment NR/R for code segment ,
  // 0: accessed
  uint8_t accessByte;

  // flags and bits 16:19 of segment limit
  // 7: granularity, 1 byte/ 4 KB, 6: descriptor size, 16-bit or
  // 32-bit (0 for Long Mode code segment), 5: Long Mode code segment; if
  // set, descriptor size bit must be 0), 4: reserved, 0-3 bits 16-19
  // of segment limit

  uint8_t segmentLimitBits16_19AndFlags;
  uint8_t segmentBaseBits24_31;  // bits 24-31 of segment base
} __attribute__((packed));

// We need one TSS and correspoding TSS descriptor in the GDT per CPU core
struct tssDescriptor {
  uint16_t segmentLimitBits0_15;  // bits 0-15 of segment limit
  uint16_t segmentBaseBits0_15;   // bits 0-15 of segment base
  uint8_t segmentBaseBits16_23;   // bits 16-23 of segment base

  // type 0-3:
  // 0x2: LDT
  // 0x9: 64-bit TSS (Available)
  // 0xB: 64-bit TSS (Busy)
  // Access nibble 4-7: 3: Present bit, 2-1: DPL/ring, 0: 0
  uint8_t typeAndAccessNibble;

  // flags: 7: granularity, 1 byte / 4KB, 5-6: 00, 4: AVL (available for use by
  // system software), 0-3 bits 16-19 of segment limit
  uint8_t segmentLimitBits16_19AndFlags;

  uint8_t segmentBaseBits24_31;   // bits 24-31 of segment base
  uint32_t segmentBaseBits32_63;  // bits 32-63 of segment base
  uint32_t reserved;              // bits 8-12 must be 0000
} __attribute__((packed));

// GDT descriptor structure (loaded to switch to GDT)
struct gdtDescriptorStruct {
  uint16_t sizeMinusOne;  // size of GDT in bytes minus 1: max value is 65535,
                          // but the GDT can be up to 65536 bytes (8192 entries)
  uint64_t offset;        // offset of GDT (32 bits or 64 bits in Long mode)
} __attribute__((packed));

// Task State Segment (TSS)
// In Long Mode, the TSS does not store information about the execution state of
// a task, instead it stores the Interrupt Stack Table
// rsp0, rsp, rsp2: stack pointers used for the stack when a privilege level
// change occurs from a lower privilege level to a higher one ist#: Interrupt
// Stack Table. tack Pointers used for the stack when an entry in the Interrupt
// Descriptor Table (IDT) has an IST value other than 0 iopb: I/O Map Base
// Address Field. Contains 16-bit offset from the base of TSS to I/O Permission
// Bit Map
struct tss {
  uint32_t reserved1;
  uint64_t rsp0;  // rsp0l, rsp0h
  uint32_t rsp1l;
  uint32_t rsp1h;
  uint32_t rsp2l;
  uint32_t rsp2h;
  uint32_t reserved2;
  uint32_t reserved3;
  uint64_t ist1;
  uint32_t ist2l;
  uint32_t ist2h;
  uint32_t ist3l;
  uint32_t ist3h;
  uint32_t ist4l;
  uint32_t ist4h;
  uint32_t ist5l;
  uint32_t ist5h;
  uint32_t ist6l;
  uint32_t ist6h;
  uint32_t ist7l;
  uint32_t ist7h;
  uint32_t reserved4;
  uint32_t reserved5;
  uint16_t reserved6;
  uint16_t iopb;
} __attribute__((packed)) __attribute__((__aligned__(8)));

// Initalize the TSS for each CPU core
// Each core has an 8KB stack growing down from
// KERNEL_SPACE_BASE_VIRTUAL_ADDRESS(0xffff800000200000) - coreID * 8 * 1024
// We set the rsp0 in the TSSs accordingly
void initTSS();

// Initialize new GDT with kernel/user mode code and data segments and one Task
// Stack Segment descriptor per core
void initGDT();

// Load GDT and CS
void loadGDT();

#endif
