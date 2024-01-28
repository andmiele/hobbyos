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

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stddef.h>
#include <stdint.h>

///*** MEMORY MANAGEMENT INFO ***///
// The kernel uses upper-half of x64 canonical virtual address space
// Advantages:
// 1) Easy to determine if a virtual address is in user or kernel space
// 2) User space programs can use lowerr addresses and assume their virtual
// address space starts at address 0

#define KERNEL_CODE_BASE 0xffff800000200000
#define KERNEL_STACK_BASE 0xffff800000200000

#define CORE_KERNEL_STACK_SIZE (8 * 1024)
// Kernel mapped to upper half of x64 virtual memory space
#define KERNEL_SPACE_BASE_VIRTUAL_ADDRESS 0xffff800000000000
// Assume kernel needs at most 1GB of physical and virtual memory
#define KERNEL_SPACE_END_VIRTUAL_ADDRESS 0xffff800040000000
#define KERNEL_PHYSICAL_MEMORY_LIMIT \
  (KERNEL_SPACE_END_VIRTUAL_ADDRESS - KERNEL_SPACE_BASE_VIRTUAL_ADDRESS)
/// The kernel uses the first 1GB of physical memory including the kernel image
/// and memory reserved for system usage as inidicated by the bios

#define MAX_N_MEMORY_REGIONS 100
#define PAGE_SIZE (4 * 1024)  // 4KB
#define PAGE_ALIGN_ADDR_UP(vAddr) \
  (((uint64_t)(vAddr)) +          \
   ((PAGE_SIZE - (((uint64_t)(vAddr)) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1)))
#define PAGE_ALIGN_ADDR_DOWN(vAddr) \
  (((uint64_t)(vAddr)) - (((uint64_t)(vAddr)) & (PAGE_SIZE - 1)))
#define VADDR_TO_PADDR(vAddr) \
  ((uint64_t)(vAddr)-KERNEL_SPACE_BASE_VIRTUAL_ADDRESS)
#define PADDR_TO_VADDR(pAddr) \
  ((uint64_t)(pAddr) + KERNEL_SPACE_BASE_VIRTUAL_ADDRESS)

/*** BIOS int 0x15, eax = 0xE820 Memory Map service ***/

#define E820_TYPE_RAM 1

// BIOS int 0x15, eax = 0xE820  memory region struct
struct memoryRegionE820 {
  uint64_t baseAddr;
  uint64_t size;
  uint32_t type;             // 1: RAM memory region
  uint32_t ACPI3Attributes;  // usually not returned by BIOS
} __attribute__((packed));

/*** OS defintions and macros ***/

// OS memory region
struct memoryRegion {
  uint64_t baseAddr;
  uint64_t size;
};

// Page struct: contains pointer to next free page struct
struct page {
  struct page *next;
};

/***  Memory allocation functions ***/

uint64_t getMemorySize();
void printFreeMemoryRegionList();

// Print pages stats
void printPagesStats();

// Populate free page list (use all free memory regions in first GB of physical
// memory)
void initMemory();
int64_t kFreePage(uint64_t addr);
void *kAllocPage(int64_t *status);
// Initialize kernel space virtual memory
void kInitVM();
// Zero out and free (add to free page list) physical pages used for 4-level
// PML4T -> PDPT -> PDT -> PT page directory tree structure and physical pages
// allocated to the process
void freeVM(uint64_t *pml4tPtr, uint64_t processTotalSize);
// Normally called by AP after BP as initialized Page Table
void loadPageTable();
// Set up page table for user space and load user process image
// processTotalSize must include processCodeSize (currently code + stack size)
int initUserSpaceVM(uint64_t *pml4tPtr, uint64_t *vAddrStart,
                    uint64_t processCodeSize, uint64_t processTotalSize);
// Set up page table for user space and copy process image from source process
int copyUserSpaceVM(uint64_t *dstPml4tPtr, uint64_t *srcPml4tPtr,
                    uint64_t *vAddrStart, uint64_t processTotalSize);

// Create 4-level 4KB Page Table structure for first 1GB of phisycal memory
// starting at KERNEL_SPACE_BASE_VIRTUAL_ADDRESS and identity map LAPIC and
// IOAPIC addresses NOTE: when this function is called in the kernel by the
// Bootstrap Processor, the first 1GB of physical memory is already mapped as a
// single 1GB page at KERNEL_SPACE_BASE_VIRTUAL_ADDRESS (loader.asm)
uint64_t *kSetupVM();

/*** Virtual Memory ***/
// There are 4 levels (optionally 5 levels, if supported by the CPU and enabled,
// in which case there is a PML5T) in the page directory tree. In long mode
// there are 512 8-byte entries per table/level PML4T: page-map level 4 table,
// one entry addresses 512GB (total of 256TB at most) PDPT: page-directory
// pointer table, one entry addresses 1GB PDT: page-directory table, one entry
// addresses 2MB PT: page table, one entry addresses 4KB

#define N_PAGE_TABLE_ENTRIES 512

// (floor(vaddr / 512GB)) % 512
#define VADDR_TO_PML4T_INDEX(v) ((v >> 39) & 0x1FF)

// (floor(vaddr / 1GB)) % 512
#define VADDR_TO_PDPT_INDEX(v) ((v >> 30) & 0x1FF)

// (floor(vaddr / 2MB)) % 512
#define VADDR_TO_PDT_INDEX(v) ((v >> 21) & 0x1FF)

// (floor(vaddr / 4KB)) % 512
#define VADDR_TO_PT_INDEX(v) ((v >> 12) & 0x1FF)

// x64 Page Map / Page Directory Entry (PDE)
typedef uint64_t pageDirectoryEntry;
#define EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(entry) \
  (((uint64_t)entry >> 12) << 12)  // clear out flags, 12 lsbs

// PDE flags
#define PAGE_DIRECTORY_ENTRY_PRESENT 1
#define PAGE_DIRECTORY_ENTRY_WRITABLE 2
#define PAGE_DIRECTORY_ENTRY_U \
  4  // 1: USER ring access; 0: SUPERVISOR ring access
// If set in PDT, 2MB pages are enabled and PT is not used
#define PAGE_DIRECTORY_SIZE_2MB 0x80

#endif
