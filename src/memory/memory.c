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

#include "memory.h"

#include "../acpi/acpi.h"          // IOAPIC addresses
#include "../graphics/graphics.h"  // frame buffer address and size
#include "../kernel.h"
#include "../lib/lib.h"
#include "../process/process.h"
#include "../stdio/stdio.h"

uint64_t *gPML4TPageMapPtr;

// Assembly lock and unlock implementations for mutex
extern void spinLock(volatile uint8_t *lock);
extern void spinUnlock(volatile uint8_t *lock);

// Static variables, zero-initialized

static volatile uint8_t
    memoryLock;  // lock from SMP access to critical sections

static uint64_t nFreePages;
static uint64_t nAllocatedPages;

// Total memory size in bytes
static uint64_t totMemorySize;

// Load cr3 register with page table address
extern void loadCR3(uint64_t pageTableAddr);

// LAPIC address ptr
extern uint64_t *gLocalApicAddress;

// IOAPIC addresses
extern uint32_t acpiNIoApics;
extern uint8_t *ioApicAddresses[MAX_N_IO_APICS_SUPPORTED];

// Defined in linker.ld script
extern struct memoryRegionE820 gMemoryMap;
extern uint32_t gNMemoryRegions;
extern char kernelEnd;

// Normally called by AP after BP as initialized Page Table
void loadPageTable() { loadCR3((uint64_t)VADDR_TO_PADDR(gPML4TPageMapPtr)); }

///*** BIOS (E820) memory map ***///

// Array of memoryRegion structures, one per free memory region obtained from
// BIOS (E820) memory map
static struct memoryRegion memoryRegions[MAX_N_MEMORY_REGIONS];
static uint64_t memoryEndAddress;

// Print pages stats
void printPagesStats() {
  printk("Free Pages: %u\n Allocated Pages: %u\n", nFreePages, nAllocatedPages);
}

// Print BIOS (E820) free memory region info
void printFreeMemoryRegionList() {
  struct memoryRegionE820 *memoryMap = &gMemoryMap;
  for (int64_t i = 0; i < gNMemoryRegions;
       i++) {  // if type field is zero invalid entry
    printk(
        "Free mem region [%d]: base address: %x, size: %u, type: %x, "
        "ACPI 3.X attributes: %x\n",
        i, memoryMap->baseAddr, memoryMap->size, memoryMap->type,
        memoryMap->ACPI3Attributes);
    ++memoryMap;
  }
}
// KERNEL ADDRESS RANGE  IN PHYSICAL MEMORY: |0x0 - 0x40000000 (1GB)|
// KERNEL SPACE:

// 0x0 -> KERNEL_SPACE_BASE_VIRTUAL_ADDRESS: 0xffff800000000000
// ..
// KERNEL IMAGE START: 0x200000 -> KERNEL_SPACE_BASE_VIRTUAL_ADDRESS + 0x200000
// ..
// KERNEL IMAGE END: kernelEnd -> KERNEL_SPACE_BASE_VIRTUAL_ADDRESS + kernelEnd
/*** KERNEL MEMORY PAGES AND SYSTEM RESERVED MEMORY REGIONS ***/
// ..
// ..
// ..
///*** ***/
// 0x40000000 (1GB) -> KERNEL_SPACE_END_VIRTUAL_ADDRESS: 0xffff800040000000

/*** KERNEL MEMORY PAGE MANAGEMENT: linked list of free pages ***/
// Each free page contains a 64-bit pointer (virtual address) to the next free
// page
static struct page freePageList;  // Head of the list (struct page consists of
                                  // just a 64-bit pointer to a struct page)

// For a given memory region add as many pages fit in it to the free page list
// as long as the page virtual address is less than
// KERNEL_SPACE_END_VIRTUAL_ADDRESS
static void freeMemoryRegion(uint64_t baseAddress, uint64_t endAddress) {
  for (uint64_t addr = PAGE_ALIGN_ADDR_UP(baseAddress);
       addr + PAGE_SIZE <= endAddress; addr += PAGE_SIZE) {
    if (addr + PAGE_SIZE <= KERNEL_SPACE_END_VIRTUAL_ADDRESS) {
      int64_t errCode = kFreePage(addr);  // add page to free page list
      if (errCode != SUCCESS) {
        printk("ERROR freeMemoryRegion: kFree failed\n");
        KERNEL_PANIC(errCode);
      }
    }
  }
}

// Initalize Kernel memory
// To be called by Bootstrap Processor
void initMemory() {
  struct memoryRegionE820 *memoryMap = &gMemoryMap;
  uint64_t nMemoryRegions = 0;
  totMemorySize = 0;
  memoryLock = 0;
  printk("initMemory:\n");

  for (int64_t i = 0; i < gNMemoryRegions; i++) {
    if (i >= MAX_N_MEMORY_REGIONS) {
      printk(
          "Number of E820 memory regions is larger than max supported number "
          "of memory regions (%d): only first %d regions will be used\n",
          MAX_N_MEMORY_REGIONS, MAX_N_MEMORY_REGIONS);
    }
    if (memoryMap[i].type == E820_TYPE_RAM) {
      memoryRegions[nMemoryRegions].baseAddr = memoryMap[i].baseAddr;
      memoryRegions[nMemoryRegions].size = memoryMap[i].size;
      totMemorySize += memoryMap[i].size;
      nMemoryRegions++;
    }
    printk("E820 region baseAddr: %x  size: %uKB  type: %u\n",
           memoryMap[i].baseAddr, memoryMap[i].size / 1024,
           (uint64_t)memoryMap[i].type);
  }

  // Populate free page list and skip kernel image
  // Only pages from free regions between physical address 0x0 and
  // KERNEL_PHYSICAL_MEMORY_LIMIT (normally 1GB, 0x40000000) are added

  for (int64_t i = 0; i < nMemoryRegions; i++) {
    uint64_t virtualBaseAddr = PADDR_TO_VADDR(memoryRegions[i].baseAddr);
    uint64_t virtualEndAddr = virtualBaseAddr + memoryRegions[i].size;
    // region is beyond kernel image in virtual memory space
    if (virtualBaseAddr > (uint64_t)&kernelEnd) {
      freeMemoryRegion(virtualBaseAddr, virtualEndAddr);
    } else  // region virtual base address is inside kernel image, allocate
            // remainder
      if (virtualEndAddr > (uint64_t)&kernelEnd) {
        freeMemoryRegion((uint64_t)&kernelEnd, virtualEndAddr);
      }
  }

  // free memory end address is address of last free page + PAGE_SIZE
  memoryEndAddress = ((uint64_t)freePageList.next) + PAGE_SIZE;
  printk("Kernel Space End address: %x\n", memoryEndAddress);
}

// Add page at address virtual address to free page list
// It popoulates the memory pointed by vAddr with a page struct
// Only pages from free regions between physical address 0x0 and
// KERNEL_PHYSICAL_MEMORY_LIMIT (normally 1GB, 0x40000000) are added

int64_t kFreePage(uint64_t vAddr) {
  if (((uint64_t)vAddr) & (PAGE_SIZE - 1)) {
    return ERR_MISALIGNED_ADDR;
  }
  if ((uint64_t)vAddr < (uint64_t)&kernelEnd) {
    return ERR_KERNEL_OVERLAP_VADDR;
  }
  if ((vAddr + PAGE_SIZE) > KERNEL_SPACE_END_VIRTUAL_ADDRESS) {
    return ERR_KERNEL_ADDR_LARGER_THAN_LIMIT;
  }

  struct page *pagePtr = (struct page *)vAddr;
  spinLock(&memoryLock);
  pagePtr->next = freePageList.next;
  freePageList.next = pagePtr;
  nFreePages++;
  if (nAllocatedPages > 0) nAllocatedPages--;

  spinUnlock(&memoryLock);
  return SUCCESS;
}

// Return void* ptr to next free page available
void *kAllocPage(int64_t *errCode) {
  *errCode = SUCCESS;
  spinLock(&memoryLock);
  struct page *pagePtr = freePageList.next;
  if (pagePtr != NULL) {
    if ((uint64_t)pagePtr & (PAGE_SIZE - 1)) {
      printk("ERROR kAllocPage: misaligned address %x\n", pagePtr);
      pagePtr = NULL;
      *errCode = ERR_MISALIGNED_ADDR;
    } else {
      if ((uint64_t)pagePtr < (uint64_t)&kernelEnd) {
        pagePtr = NULL;
        printk("ERROR kAllocPage: address inside kernel image area\n");
        *errCode = ERR_KERNEL_OVERLAP_VADDR;
      } else {
        if (((uint64_t)pagePtr) + PAGE_SIZE >
            KERNEL_SPACE_END_VIRTUAL_ADDRESS) {
          pagePtr = NULL;
          printk("ERROR kAllocPage: address beyond kernel space limit\n");
          *errCode = ERR_KERNEL_ADDR_LARGER_THAN_LIMIT;
        } else {
          if (pagePtr != NULL) freePageList.next = pagePtr->next;
          nAllocatedPages++;
          if (nFreePages > 0) nFreePages--;
        }
      }
    }

  } else  // freeList page ptr is NULL
  {
    printk("ERROR kAllocPage: NULL free page list next pointer\n");
    *errCode = ERR_ALLOC_FAILED;
  }
  spinUnlock(&memoryLock);
  return (void *)pagePtr;
}

///*** x64 VIRTUAL MEMORY MANAGEMENT ***///
// There are 4 levels (optionally 5 levels if supported by the CPU and enabled
// in which case there is a PML5T) in the page directory tree In long mode there
// are 512 8-byte entries per table/level PML4T: page-map level 4 table, one
// entry addresses 512GB (total of 256TB at most) PDPT: page-directory pointer
// table, one entry addresses 1GB PDT: page-directory table, one entry addresses
// 2MB PT: page table, one entry addresses 4KB

// WE USE 4KB pages: PML4T -> PDPT -> PDT -> PT

// Returns pointer to virtual address of PDPT (pointer to first entry) for input
// virtual address if PML4T entry pointing to it exists or NULL if it does not
// exist
static uint64_t *getPDPTPointer(uint64_t *PML4TPtr, uint64_t vAddr) {
  uint64_t PML4TEntryIndex = VADDR_TO_PML4T_INDEX(vAddr);
  uint64_t *PDPTPtr = NULL;

  if ((PML4TPtr[PML4TEntryIndex] & PAGE_DIRECTORY_ENTRY_PRESENT)) {
    PDPTPtr = (uint64_t *)PADDR_TO_VADDR(
        EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(PML4TPtr[PML4TEntryIndex]));
  }
  return PDPTPtr;
}

// Return pointer to virtual address of PDT (first entry) for input virtual
// address if PDPT entry pointing to it exists or NULL if it does not exist
static uint64_t *getPDTPointer(uint64_t *PML4TPtr, uint64_t vAddr) {
  uint64_t PDPTEntryIndex = VADDR_TO_PDPT_INDEX(vAddr);
  uint64_t *PDPTPtr = NULL;
  uint64_t *PDTPtr = NULL;

  PDPTPtr = getPDPTPointer(PML4TPtr, vAddr);

  if (PDPTPtr != NULL &&
      (PDPTPtr[PDPTEntryIndex] & PAGE_DIRECTORY_ENTRY_PRESENT)) {
    PDTPtr = (uint64_t *)PADDR_TO_VADDR(
        EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(PDPTPtr[PDPTEntryIndex]));
  }
  return PDTPtr;
}

// Return pointer to virtual address of PT (first entry) for input virtual
// address if PDT entry pointing to it exists or NULL if it does not exist
static uint64_t *getPTPointer(uint64_t *PML4TPtr, uint64_t vAddr) {
  uint64_t PDTEntryIndex = VADDR_TO_PDT_INDEX(vAddr);
  uint64_t *PDTPtr = NULL;
  uint64_t *PTPtr = NULL;

  PDTPtr = getPDTPointer(PML4TPtr, vAddr);

  if (PDTPtr != NULL &&
      (PDTPtr[PDTEntryIndex] & PAGE_DIRECTORY_ENTRY_PRESENT)) {
    PTPtr = (uint64_t *)PADDR_TO_VADDR(
        EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(PDTPtr[PDTEntryIndex]));
  }
  return PTPtr;
}

// If a PDT entry pointing to a PT (first
// entry) for the input virtual address does not exist: allocate a page for
// PT, zero initialize the page, create PDT entry and return pointer to
// zero-initialized page
// Recursively allocate PML4T, PDPT and PDT for vAddr if they do not exist
static uint64_t *createPDTEntryAllocatePT(uint64_t *PML4TPtr, uint64_t vAddr,
                                          uint64_t attributes) {
  uint64_t PML4TEntryIndex = VADDR_TO_PML4T_INDEX(vAddr);
  uint64_t PDPTEntryIndex = VADDR_TO_PDPT_INDEX(vAddr);
  uint64_t PDTEntryIndex = VADDR_TO_PDT_INDEX(vAddr);
  uint64_t *PDPTPtr = NULL;
  uint64_t *PDTPtr = NULL;
  uint64_t *PTPtr = NULL;
  int64_t errCode = SUCCESS;

  if ((PML4TPtr[PML4TEntryIndex] & PAGE_DIRECTORY_ENTRY_PRESENT)) {
    PDPTPtr = (uint64_t *)PADDR_TO_VADDR(
        EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(PML4TPtr[PML4TEntryIndex]));
  } else {
    PDPTPtr = kAllocPage(&errCode);
    if (PDPTPtr != NULL) {
      memset(PDPTPtr, 0, PAGE_SIZE);
    } else {
      printk("ERROR createPDTEntryAllocatePT: kAllocPage for PDPT failed\n");
      KERNEL_PANIC(errCode);
    }
    PML4TPtr[PML4TEntryIndex] = VADDR_TO_PADDR(PDPTPtr) | attributes;
  }

  if ((PDPTPtr[PDPTEntryIndex] & PAGE_DIRECTORY_ENTRY_PRESENT)) {
    PDTPtr = (uint64_t *)PADDR_TO_VADDR(
        EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(PDPTPtr[PDPTEntryIndex]));
  } else {
    PDTPtr = kAllocPage(&errCode);
    if (PDTPtr != NULL) {
      memset(PDTPtr, 0, PAGE_SIZE);
    } else {
      printk("ERROR createPDTEntryAllocatePT: kAllocPage for PDT failed\n");
      KERNEL_PANIC(errCode);
    }
    PDPTPtr[PDPTEntryIndex] = VADDR_TO_PADDR(PDTPtr) | attributes;
  }

  // if PDT entry (PDPT) exists and PDT entry is not present
  if (PDTPtr[PDTEntryIndex] & PAGE_DIRECTORY_ENTRY_PRESENT) {
    PTPtr = (uint64_t *)PADDR_TO_VADDR(
        EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(PDTPtr[PDTEntryIndex]));
  } else {
    uint64_t f = (uint64_t)freePageList.next;
    PTPtr = kAllocPage(&errCode);
    if (PTPtr != NULL) {
      memset(PTPtr, 0, PAGE_SIZE);
    } else {
      printk("ERROR createPDTEntryAllocatePT: kAllocPage for PT failed %x\n",
             f);
      KERNEL_PANIC(errCode);
    }
    PDTPtr[PDTEntryIndex] = VADDR_TO_PADDR(PTPtr) | attributes;
  }
  return PTPtr;
}

// Create page table mappings for all physical pages between pStartAddr and
// (pStartAddr + vEndAddr - vStartAddr) to virtual pages between vStartAddr and
// vEndAddr (after aligning virtual addresses to page boundaries)
// pStartAddr must be page-aligned
int kMapPagesForAddrRange(uint64_t *pml4tPtr, uint64_t vStartAddr,
                          uint64_t vEndAddr, uint64_t pStartAddr,
                          uint64_t pageAttributes) {
  uint64_t vStartAddrAligned = PAGE_ALIGN_ADDR_DOWN(vStartAddr);
  uint64_t vEndAddrAligned = PAGE_ALIGN_ADDR_UP(vEndAddr);

  if (vEndAddr < vStartAddr) {
    printk("ERROR kMapPagesForAddrRange: negative address range\n");
    return ERR_NEG_ADDR_RANGE;
  }
  if (pStartAddr & (PAGE_SIZE - 1)) {  // % PAGE_SIZE
    printk("ERROR kMapPagesForAddrRange: pStartAddr is not page-aligned\n");
    return ERR_MISALIGNED_ADDR;
  }
  if (vEndAddrAligned > KERNEL_SPACE_END_VIRTUAL_ADDRESS) {
    printk(
        "ERROR kMapPagesForAddrRange: vEndAddrAligned larger than kernel "
        "limit\n");
    return ERR_KERNEL_ADDR_LARGER_THAN_LIMIT;
  }
  do {
    uint64_t *ptPtr = getPTPointer(pml4tPtr, vStartAddrAligned);
    if (ptPtr == NULL) {
      ptPtr =
          createPDTEntryAllocatePT(pml4tPtr, vStartAddrAligned, pageAttributes);
    }
    if (ptPtr == NULL) {
      printk("ERROR kMapPagesForAddrRange: allocating page for PT failed\n");
      return ERR_ALLOC_FAILED;
    }
    uint64_t ptIndex = VADDR_TO_PT_INDEX(vStartAddrAligned);
    if (ptPtr[ptIndex] & PAGE_DIRECTORY_ENTRY_PRESENT) {
      printk(
          "ERROR kMapPagesForAddrRange PT: attempt to map a page that was "
          "already "
          "mapped: %d (%x) \n",
          ptIndex, vStartAddrAligned);
      return ERR_PAGE_IS_ALREADY_MAPPED;
    }
    ptPtr[ptIndex] = pStartAddr | pageAttributes;
    vStartAddrAligned += PAGE_SIZE;
    pStartAddr += PAGE_SIZE;

  } while (vStartAddrAligned + PAGE_SIZE <= vEndAddrAligned);

  return 0;
}

// Create 4-level 4KB Page Table structure for first 1GB of phisycal memory
// starting at KERNEL_SPACE_BASE_VIRTUAL_ADDRESS and identity map LAPIC and
// IOAPIC addresses NOTE: when this function is called in the kernel by the
// Bootstrap Processor, the first 1GB of physical memory is already mapped as a
// single 1GB page at KERNEL_SPACE_BASE_VIRTUAL_ADDRESS (loader.asm)
uint64_t *kSetupVM() {
  // Allocage page for PML4T (Page Map Level-4 Table)

  int64_t errCode = SUCCESS;
  uint64_t *pml4TPageMapPtr = kAllocPage(&errCode);
  if (pml4TPageMapPtr == NULL) {
    printk("KSetupVM ERROR: kAllocPage for PML4T failed\n");
    KERNEL_PANIC(errCode);
  }

  //   printk("High memory 1GB Kernel mapping %x\n",
  //   KERNEL_SPACE_BASE_VIRTUAL_ADDRESS);
  // zero out PML4T page
  memset(pml4TPageMapPtr, 0, PAGE_SIZE);
  errCode = kMapPagesForAddrRange(
      pml4TPageMapPtr, KERNEL_SPACE_BASE_VIRTUAL_ADDRESS,
      KERNEL_SPACE_END_VIRTUAL_ADDRESS,
      VADDR_TO_PADDR(KERNEL_SPACE_BASE_VIRTUAL_ADDRESS),
      PAGE_DIRECTORY_ENTRY_PRESENT | PAGE_DIRECTORY_ENTRY_WRITABLE);
  if (errCode != 0) {
    printk("ERROR kSetupVM: kMapPagesForAddrRange failed\n");
    KERNEL_PANIC(errCode);
  }

  //   printk("Identity mapping for LAPIC address: %x\n",
  //          PAGE_ALIGN_ADDR_DOWN(gLocalApicAddress));
  errCode = kMapPagesForAddrRange(
      pml4TPageMapPtr, PAGE_ALIGN_ADDR_DOWN(gLocalApicAddress),
      PAGE_ALIGN_ADDR_DOWN(gLocalApicAddress) + PAGE_SIZE,
      PAGE_ALIGN_ADDR_DOWN(gLocalApicAddress),
      PAGE_DIRECTORY_ENTRY_PRESENT | PAGE_DIRECTORY_ENTRY_WRITABLE);
  if (errCode != 0) {
    printk(
        "ERROR kSetupVM: LAPIC address identity mapping in "
        "kMapPagesForAddrRange failed\n");
    KERNEL_PANIC(errCode);
  }

  for (int i = 0; i < acpiNIoApics; i++) {
    //     printk("Identity mapping for IOAPIC address: %x\n",
    //           PAGE_ALIGN_ADDR_DOWN(ioApicAddresses[i]));
    errCode = kMapPagesForAddrRange(
        pml4TPageMapPtr, PAGE_ALIGN_ADDR_DOWN(ioApicAddresses[i]),
        PAGE_ALIGN_ADDR_DOWN(ioApicAddresses[i]) + PAGE_SIZE,
        PAGE_ALIGN_ADDR_DOWN(ioApicAddresses[i]),
        PAGE_DIRECTORY_ENTRY_PRESENT | PAGE_DIRECTORY_ENTRY_WRITABLE);
    if (errCode != 0) {
      printk(
          "ERROR kSetupVM: LAPIC address identity mapping in "
          "kMapPagesForAddrRange failed\n");
      KERNEL_PANIC(errCode);
    }
  }

  // Map VBE frame buffer
  size_t fbSize = getFrameBufferSize();
  uint64_t fbAddress = getFrameBufferAddress();

  //  printk("Identity mapping for VBE frame buffer: %x (%u bytes)\n",
  //          PAGE_ALIGN_ADDR_DOWN(fbAddress), fbSize);
  errCode = kMapPagesForAddrRange(
      pml4TPageMapPtr, PAGE_ALIGN_ADDR_DOWN(fbAddress),
      PAGE_ALIGN_ADDR_UP(fbAddress + fbSize), PAGE_ALIGN_ADDR_DOWN(fbAddress),
      PAGE_DIRECTORY_ENTRY_PRESENT | PAGE_DIRECTORY_ENTRY_WRITABLE);
  if (errCode != 0) {
    printk(
        "ERROR kSetupVM: VBE frame buffer identity mapping in "
        "kMapPagesForAddrRange failed\n");
    KERNEL_PANIC(errCode);
  }

  return pml4TPageMapPtr;
}
// Initialize kernel space virtual memory
void kInitVM() {
  gPML4TPageMapPtr = kSetupVM();
  loadCR3((uint64_t)VADDR_TO_PADDR(gPML4TPageMapPtr));
  printk("Kernel Virtual memory initialization complete!\n");
}

// Set up page table for user space and load user process image
// processTotalSize must include processCodeSize (currently code + stack size)
int initUserSpaceVM(uint64_t *pml4tPtr, uint64_t *processImageBuffer,
                    uint64_t processCodeSize, uint64_t processTotalSize) {
  int64_t errCode = SUCCESS;

  if (processCodeSize > processTotalSize) {
    printk("ERROR initUserSpaceVM: processCodeSize > processTotalSize\n");
    KERNEL_PANIC(errCode);
  }

  uint64_t nProcessPages =
      (processTotalSize / PAGE_SIZE) + ((processTotalSize % PAGE_SIZE) != 0);
  uint64_t nProcessCodePages =
      (processCodeSize / PAGE_SIZE) + ((processCodeSize % PAGE_SIZE) != 0);

  for (uint64_t i = 0; i < nProcessPages; i++) {
    uint64_t *page = kAllocPage(&errCode);
    if (page != NULL) {
      memset(page, 0, PAGE_SIZE);

      // Using this function here is a bit of an  overkill as we map only one
      // physical page at a time
      errCode = kMapPagesForAddrRange(
          pml4tPtr, USER_PROGRAM_COUNTER + i * PAGE_SIZE,
          USER_PROGRAM_COUNTER + (i + 1) * PAGE_SIZE, VADDR_TO_PADDR(page),
          PAGE_DIRECTORY_ENTRY_PRESENT | PAGE_DIRECTORY_ENTRY_WRITABLE |
              PAGE_DIRECTORY_ENTRY_U);

      if (errCode == SUCCESS) {
        if (i < nProcessCodePages) {
          uint64_t size = PAGE_SIZE;
          // If last page, adjust size if process code size is not divisible by
          // PAGE_SIZE
          if ((i == (nProcessCodePages - 1)) && (processCodeSize % PAGE_SIZE)) {
            size = processCodeSize % PAGE_SIZE;
          }

          uint64_t *vAddr =
              processImageBuffer + (i * PAGE_SIZE / sizeof(uint64_t));
          memcpy(page, vAddr, size);
        }
      } else {
        errCode = kFreePage((uint64_t)page);
        if (errCode != SUCCESS) {
          printk(
              "ERROR initUserSpaceVM: kFreePage failed after "
              "kMapPagesForAddrRange error\n");
          KERNEL_PANIC(errCode);
        }
      }

    } else {
      return ERR_ALLOC_FAILED;
    }
  }
  return errCode;
}

// Set up page table for user space and copy process image from source process
int copyUserSpaceVM(uint64_t *dstPml4tPtr, uint64_t *srcPml4tPtr,
                    uint64_t *processImageBuffer, uint64_t processTotalSize) {
  int64_t errCode = SUCCESS;

  uint64_t nProcessPages =
      (processTotalSize / PAGE_SIZE) + ((processTotalSize % PAGE_SIZE) != 0);

  for (uint64_t i = 0; i < nProcessPages; i++) {
    uint64_t *page = kAllocPage(&errCode);
    if (page != NULL) {
      memset(page, 0, PAGE_SIZE);

      // Using this function here is a bit of an  overkill as we map only one
      // physical page at a time

      errCode = kMapPagesForAddrRange(
          dstPml4tPtr, USER_PROGRAM_COUNTER + i * PAGE_SIZE,
          USER_PROGRAM_COUNTER + (i + 1) * PAGE_SIZE, VADDR_TO_PADDR(page),
          PAGE_DIRECTORY_ENTRY_PRESENT | PAGE_DIRECTORY_ENTRY_WRITABLE |
              PAGE_DIRECTORY_ENTRY_U);

      if (errCode == SUCCESS) {
        uint64_t size = PAGE_SIZE;
        // If last page, adjust size if processTotalSize is not divisible by
        // PAGE_SIZE
        if ((i == (nProcessPages - 1)) && (processTotalSize % PAGE_SIZE)) {
          size = processTotalSize % PAGE_SIZE;
        }

        uint64_t *vAddr =
            processImageBuffer + (i * PAGE_SIZE / sizeof(uint64_t));
        uint64_t *ptPtr = getPTPointer(srcPml4tPtr, (uint64_t)vAddr);
        if (ptPtr == NULL) {
          printk("ERROR copyUserSpaceVM: getPTPointer returned NULL\n");
          return ERR_VM;
        }
        uint64_t ptIndex = VADDR_TO_PT_INDEX((uint64_t)vAddr);
        if (!(ptPtr[ptIndex] & PAGE_DIRECTORY_ENTRY_PRESENT)) {
          printk("ERROR copyUserSpaceVM: PT entry present page flag not set\n");
          return ERR_VM;
        }

        memcpy(page, vAddr, size);
      } else {
        errCode = kFreePage((uint64_t)page);
        if (errCode != SUCCESS) {
          printk(
              "ERROR copyUserSpaceVM: kFreePage failed after "
              "kMapPagesForAddrRange error\n");
          KERNEL_PANIC(errCode);
        }
      }

    } else {
      printk("ERROR copyUserSpaceVM: kAlloc failed\n");
      return ERR_ALLOC_FAILED;
    }
  }

  return errCode;
}

// If there there are virtual pages that are mapped to phyisical pages in the
// address range, add them to to the free list and clear the PDT entry
int64_t kFreePagesInAddrRange(uint64_t *pml4tPtr, uint64_t vStartAddr,
                              uint64_t vEndAddr) {
  if (vStartAddr & (PAGE_SIZE - 1)) {  // % PAGE_SIZE
    printk("ERROR kFreePagesInAddrRange: vStartAddr is not page-aligned\n");
    return ERR_MISALIGNED_ADDR;
  }
  if (vEndAddr & (PAGE_SIZE - 1)) {  // % PAGE_SIZE
    printk("ERROR kFreePagesInAddrRange: vEndAddr is not page-aligned\n");
    return ERR_MISALIGNED_ADDR;
  }
  if (vEndAddr < vStartAddr) {
    printk("ERROR kFreePagesInAddrRange: negative address range\n");
    return ERR_NEG_ADDR_RANGE;
  }

  do {
    uint64_t ptIndex = VADDR_TO_PT_INDEX(vStartAddr);
    uint64_t *ptPtr = getPTPointer(pml4tPtr, vStartAddr);
    if (ptPtr) {
      if ((ptPtr[ptIndex] & PAGE_DIRECTORY_ENTRY_PRESENT)) {
        kFreePage(PADDR_TO_VADDR(
            EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(ptPtr[ptIndex])));
        ptPtr[ptIndex] = 0;
      }
    }
    vStartAddr += PAGE_SIZE;
  } while (vStartAddr + PAGE_SIZE <= vEndAddr);
  return SUCCESS;
}

// For each of the 512 PML4T entries, for each of the 512 PDPT entries, for each
// of the 512 PDT entries free the page containing one PT pointed to by the PDT
// entry and zero out PDT entry
static void kFreePT(uint64_t *pml4tPtr) {
  for (int i = 0; i < N_PAGE_TABLE_ENTRIES; i++) {
    if (pml4tPtr[i] & PAGE_DIRECTORY_ENTRY_PRESENT) {
      uint64_t *pdptPtr = (uint64_t *)PADDR_TO_VADDR(
          EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(pml4tPtr[i]));
      for (int ii = 0; ii < N_PAGE_TABLE_ENTRIES; ii++) {
        if ((pdptPtr[ii] & PAGE_DIRECTORY_ENTRY_PRESENT)) {
          uint64_t *pdtPtr = (uint64_t *)PADDR_TO_VADDR(
              EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(pdptPtr[ii]));
          for (int iii = 0; iii < N_PAGE_TABLE_ENTRIES; iii++) {
            if (pdtPtr[iii] & PAGE_DIRECTORY_ENTRY_PRESENT) {
              uint64_t errCode = kFreePage(PADDR_TO_VADDR(
                  EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(pdtPtr[iii])));

              if (errCode != SUCCESS) {
                printk("ERROR kFreePT kFreePage failed\n");
                KERNEL_PANIC(errCode);
              }
              pdtPtr[iii] = 0;
            }
          }
        }
      }
    }
  }
}

// For each of the 512 PML4T entries, for each of the 512 PDPT entries free the
// page containing one PDT pointed to by the PDPT entry and zero out PDTP entry
static void kFreePDT(uint64_t *pml4tPtr) {
  for (int i = 0; i < N_PAGE_TABLE_ENTRIES; i++) {
    if (pml4tPtr[i] & PAGE_DIRECTORY_ENTRY_PRESENT) {
      uint64_t *pdptPtr = (uint64_t *)PADDR_TO_VADDR(
          EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(pml4tPtr[i]));
      for (int ii = 0; ii < N_PAGE_TABLE_ENTRIES; ii++) {
        if (pdptPtr[ii] & PAGE_DIRECTORY_ENTRY_PRESENT) {
          uint64_t errCode = kFreePage(PADDR_TO_VADDR(
              EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(pdptPtr[ii])));
          if (errCode != SUCCESS) {
            printk("ERROR kFreePDT kFreePage failed\n");
            KERNEL_PANIC(errCode);
          }
          pdptPtr[ii] = 0;
        }
      }
    }
  }
}

// For each of the 512 PML4T entries free the page containing one PDPT pointed
// to by the PML4T entry and zero out PML4T entry
static void kFreePDPT(uint64_t *pml4tPtr) {
  for (int i = 0; i < N_PAGE_TABLE_ENTRIES; i++) {
    if (pml4tPtr[i] & PAGE_DIRECTORY_ENTRY_PRESENT) {
      uint64_t errCode = kFreePage(
          PADDR_TO_VADDR(EXTRACT_PAGE_DIRECTORY_ENTRY_ADDRESS(pml4tPtr[i])));
      if (errCode != SUCCESS) {
        printk("ERROR kFreePDPT kFreePage failed\n");
        KERNEL_PANIC(errCode);
      }
      pml4tPtr[i] = 0;
    }
  }
}

// Zero out and free (add to free page list) pages used for 3-level PML4T ->
// PDPT -> PDT page directory tree strucure
void freeVM(uint64_t *pml4tPtr, uint64_t processTotalSize) {
  uint64_t errCode = kFreePagesInAddrRange(
      pml4tPtr, USER_PROGRAM_COUNTER, USER_PROGRAM_COUNTER + processTotalSize);
  if (errCode != SUCCESS) {
    printk("ERROR freeVM kFreePagesInAddrRange failed\n");
    KERNEL_PANIC(errCode);
  }
  kFreePT(pml4tPtr);
  kFreePDT(pml4tPtr);
  kFreePDPT(pml4tPtr);
  // free page containing PML4T
  errCode = kFreePage((uint64_t)pml4tPtr);
  if (errCode != SUCCESS) {
    printk("ERROR kFreeVM kFreePage for PML4T failed\n");
    KERNEL_PANIC(errCode);
  }
}

uint64_t getMemorySize() { return totMemorySize; }
