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

#include "gdt.h"

#include "../acpi/acpi.h"  // MAX_N_CORES_SUPPORTED
#include "../lib/lib.h"    // memset
#include "../memory/memory.h"
#include "../stdio/stdio.h"  // printk
// gdt:
// NULL descriptor
// KERNEL code segment descriptor
// KERNEL data segment descriptor
// USER code segment descriptor
// USER data segment descriptor
// MAX_N_CORES_SUPPORTED * TSS descriptor
// ..
// ..
uint8_t gdt[GDT_SIZE];

// GDT descriptor structure (loaded to switch to GDT)
struct gdtDescriptorStruct gdtDescStruct;

// Array of TSSs; one per CPU core
struct tss tssArray[MAX_N_CORES_SUPPORTED];

// Defined in gdt.asm
// Load GDT and set CS (code segment selector) to gdtCodeSegmentDescIndex
extern void loadGDTAndCS(void *gdtDescStructPtr,
                         uint64_t gdtCodeSegmentDescIndex);

// Initalize the TSS for each CPU core
// After switching to Long Mode (64-bit) during the boot procedure, each core
// has an 8KB stack growing down from
// KERNEL_STACK_BASE(0xffff800000200000) - coreID * 1024 * 8
// After switching to ring0 from ring3 upon an interrupt, each core will use the
// same stack pointer initially but this will be replaced by a per-process ring0
// stack as as a process starts runnig on the core
// In case of an non-maskable interrupt (NMI) or invalid TSS exception or
// other iterrupt handlers that should not rely on the per-process ring0 stack
// being valid we want the  8KB per-core stack to be used upon switching to
// ring0 so we set the TSS Interrupt Stack Table (IST1) to
// KERNEL_STACK_BASE(0xffff800000200000) - coreID * 1024 * 8
// and we set the IDT descriptors for these interrupts accordingly
void initTSS() {
  for (int i = 0; i < MAX_N_CORES_SUPPORTED; i++) {
    memset(&tssArray[i], 0, sizeof(struct tss));
    uint64_t rsp = KERNEL_STACK_BASE - (i * CORE_KERNEL_STACK_SIZE);
    tssArray[i].rsp0 = rsp;
    tssArray[i].ist1 = rsp;
    tssArray[i].iopb =
        sizeof(struct tss);  // set iopb to tss size: iopb not used
  }
  printk("Per core TSS initialized!\n");
}

static void populateGDTDescriptor(struct gdtDescriptor *desc,
                                  uint32_t segmentBase, uint32_t segmentLimit,
                                  uint8_t accessByte, uint8_t flagsNibble) {
  desc->segmentLimitBits0_15 = segmentLimit & 0xFFFF;
  desc->segmentBaseBits0_15 = segmentBase & 0xFFFF;
  desc->segmentBaseBits16_23 = (segmentBase >> 16) & 0xFF;
  desc->accessByte = accessByte;
  desc->segmentLimitBits16_19AndFlags =
      ((segmentLimit >> 16) & 0xF) | (flagsNibble << 4);
  desc->segmentBaseBits24_31 = segmentBase >> 24;
}

static void populateTSSDescriptor(struct tssDescriptor *desc,
                                  uint64_t segmentBase, uint32_t segmentLimit,
                                  uint8_t typeNibble, uint8_t accessNibble,
                                  uint8_t flagsNibble) {
  desc->segmentLimitBits0_15 = segmentLimit & 0xFFFF;
  desc->segmentBaseBits0_15 = segmentBase & 0xFFFF;
  desc->segmentBaseBits16_23 = (segmentBase >> 16) & 0xFF;
  desc->typeAndAccessNibble = (typeNibble & 0xF) | (accessNibble << 4);
  desc->segmentLimitBits16_19AndFlags =
      ((segmentLimit >> 16) & 0xF) | (flagsNibble << 4);
  desc->segmentBaseBits24_31 = segmentBase >> 24;
  desc->segmentBaseBits32_63 = segmentBase >> 32;
  desc->reserved = 0;
}

// Initialize new GDT with kernel/user mode code and data segments and one Task
// Stack Segment descriptor per core
void initGDT() {
  struct gdtDescriptor *gdtPtr = (struct gdtDescriptor *)gdt;
  struct tssDescriptor *gdtTSSPtr =
      (struct tssDescriptor *)(gdt + N_GDT_SEGMENT_DESCRIPTORS *
                                         sizeof(struct gdtDescriptor));

  // Populate GDT segment descriptors

  // NULL
  populateGDTDescriptor(&gdtPtr[0], 0, 0, 0, 0);
  // KERNEL CODE SEGMENT
  populateGDTDescriptor(&gdtPtr[1], 0, 0x0,
                        GDT_DESC_ACCESS_BYTE_PRESENT |
                            GDT_DESC_ACCESS_BYTE_CODE_DATA_TYPE |
                            GDT_DESC_ACCESS_BYTE_EXECUTABLE,
                        GDT_DESC_FLAGS_LONG_MODE_CODE_DESCRIPTOR);
  // KERNEL DATA SEGMENT
  populateGDTDescriptor(&gdtPtr[2], 0, 0x0,
                        GDT_DESC_ACCESS_BYTE_PRESENT |
                            GDT_DESC_ACCESS_BYTE_CODE_DATA_TYPE |
                            GDT_DESC_ACCESS_BYTE_DATA_SEGMENT_WRITABLE,
                        0);
  // NULL
  populateGDTDescriptor(&gdtPtr[3], 0, 0, 0, 0);

  // USER CODE SEGMENT
  populateGDTDescriptor(
      &gdtPtr[5], 0, 0x0,
      GDT_DESC_ACCESS_BYTE_PRESENT | GDT_DESC_ACCESS_BYTE_DPL_RING3 |
          GDT_DESC_ACCESS_BYTE_CODE_DATA_TYPE | GDT_DESC_ACCESS_BYTE_EXECUTABLE,
      GDT_DESC_FLAGS_LONG_MODE_CODE_DESCRIPTOR);
  // USER DATA SEGMENT
  populateGDTDescriptor(&gdtPtr[4], 0, 0x0,
                        GDT_DESC_ACCESS_BYTE_PRESENT |
                            GDT_DESC_ACCESS_BYTE_DPL_RING3 |
                            GDT_DESC_ACCESS_BYTE_CODE_DATA_TYPE |
                            GDT_DESC_ACCESS_BYTE_DATA_SEGMENT_WRITABLE,
                        0);

  // Populate TSS descriptors
  for (int i = 0; i < MAX_N_CORES_SUPPORTED; i++) {
    populateTSSDescriptor(&gdtTSSPtr[i], (uint64_t)(&tssArray[i]),
                          sizeof(struct tss) - 1, TSS_DESC_TYPE_TSS_AVAILABLE,
                          TSS_DESC_ACCESS_NIBBLE_PRESENT, 0);
  }

  // Set GDT descriptor struct
  gdtDescStruct.sizeMinusOne = GDT_SIZE - 1;
  gdtDescStruct.offset = (uint64_t) & (gdt[0]);
  loadGDTAndCS(&gdtDescStruct, CODE_SEG_SELECTOR);
  printk("GDT initialized\n");
  printk("KERNEL CODE SEGMENT: %x\n", gdtPtr[1]);
  printk("KERNEL DATA SEGMENT: %x\n", gdtPtr[2]);
  printk("USER CODE SEGMENT: %x\n", gdtPtr[4]);
  printk("USER DATA SEGMENT: %x\n", gdtPtr[5]);
}

// load GDT and CS
void loadGDT() { loadGDTAndCS(&gdtDescStruct, CODE_SEG_SELECTOR); }
