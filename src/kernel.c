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

#include "kernel.h"

#include <stdarg.h>
#include <stddef.h>

#include "acpi/acpi.h"
#include "drivers/disk.h"
#include "fat16/fat16.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "io/io.h"
#include "memory/memory.h"
#include "process/process.h"
#include "stdio/stdio.h"
#include "syscall/syscall.h"
#include "vga/vga.h"

// syscall/syscall.asm
extern void enableSysCall();

extern void loadTaskRegister(uint64_t tss);

// BSS start and end symbols, defined in linker.ld
extern uint8_t bssStart;
extern uint8_t bssEnd;

extern uint64_t gActiveCpuCount;

extern volatile uint8_t fat16Lock;    // lock for FAT16 shared structures
extern volatile uint8_t processLock;  // lock for process shared structures

const char *kernelStartString = "Kernel Started!\n";

// Print reason for kernel error given integer error code
void printKernelError(int64_t errCode) {
  switch (errCode) {
    case SUCCESS:
      printk("%d: Success!\n", errCode);
      break;
    case ERR_MISALIGNED_ADDR:
      printk("%d: PAGE BOUNDARY MISALIGNED ADDRESS ERROR!\n", errCode);
      break;
    case ERR_KERNEL_OVERLAP_VADDR:
      printk("%d: VIRTUAL ADDRESS WITHIN KERNEL IMAGE RANGE ERROR!\n", errCode);
      break;
    case ERR_KERNEL_ADDR_LARGER_THAN_LIMIT:
      printk(
          "%d: ADDRESS IS LARGER THAN KERNEL UPPER LIMIT "
          "(normally, 1GB) ERROR!\n",
          errCode);
      break;
    case ERR_NEG_ADDR_RANGE:
      printk("%d: NEGATIVE ADDRESS RANGE ERROR!\n", errCode);
      break;
    case ERR_ALLOC_FAILED:
      printk("%d: PAGE ALLOCATION FAILED ERROR!\n", errCode);
      break;
    case ERR_PAGE_IS_ALREADY_MAPPED:
      printk("%d: ATTEMPT TO OVERWRITE PRESENT PAGE ENTRY ERROR!\n", errCode);
      break;
    case ERR_PAGE_IS_NOT_PRESENT:
      printk("%d: PAGE IS NOT PRESENT ERROR!\n", errCode);
      break;
    case ERR_PROCESS:
      printk("%d: PROCESS ERROR!\n", errCode);
      break;
    case ERR_SCHEDULER:
      printk("%d: SCHEDULER ERROR!\n", errCode);
      break;
    case ERR_FAT16:
      printk("%d: FAT16 FILE SYSTEM ERROR!\n", errCode);
      break;
    case ERR_VM:
      printk("%d: VIRTUAL MEMORY PAGE TABLE ERROR!\n", errCode);
      break;
    default:
      printk("%d: UNKOWN ERROR CODE!\n", errCode);
      break;
  }
}

void kernelStart() {
  // Zero out BSS section (static variables)
  // bssEnd and bssStart are defined in linked script
  size_t bssSize = ((size_t)(&bssEnd)) - ((size_t)(&bssStart));
  memset(&bssStart, 0, bssSize);
  fat16Lock = 0;
  processLock = 0;
  // Initalize non-static SMP locks

  // Bootstrap processor (BP), currently running
  gActiveCpuCount = 1;
  vgaInit();
  printk(kernelStartString);
  acpiInit();
  ioAPICInit();
  localAPICInit();
  initializeIDT();
  /*
    // Keyboard initialization
    outb(PS2_COMMAND_IO_PORT, PS2_DISABLE_FIRST_PORT_CMD);
    outb(PS2_COMMAND_IO_PORT,
         PS2_DISABLE_SECOND_PORT_CMD); // ignored if not supported

    // flush device buffer
    inb(PS2_DATA_IO_PORT);
    outb(PS2_COMMAND_IO_PORT, PS2_READ_BYTE_0_CMD);
    uint8_t status = inb(PS2_DATA_IO_PORT);
    uint8_t secondCh =
        (status & 0b00100000) == 0b00100000; // if 0 no second channel
    if (secondCh == 0) {
      printk("Keyboard initialization: there is no second channel\n");
    }
    status |= 0b01000011; //...0x11 enabled irqs for both ports
    outb(PS2_COMMAND_IO_PORT, PS2_WRITE_NEXT_BYTE_0_CMD);
    outb(PS2_DATA_IO_PORT, status);
    // finsh PS/2 initalization by reenabling devices
    outb(PS2_COMMAND_IO_PORT, PS2_ENABLE_FIRST_PORT_CMD);
    outb(PS2_COMMAND_IO_PORT,
         PS2_ENABLE_SECOND_PORT_CMD); // ignored if unsupported
    outb(PS2_COMMAND_IO_PORT, PS2_RESET_CMD);
  */
  printFreeMemoryRegionList();
  initMemory();
  initTSS();
  initGDT();
  kInitVM();
  loadTaskRegister(LONG_MODE_FIRST_TSS);

  initSystemCalls();
  initStartupProcesses();
  startIdleProcess();
  smpInit();
  printk("Active cores count: %d\n", gActiveCpuCount);

  /*
  int64_t d = 19;
  int64_t dd = -199999;
  uint64_t u = 555;
  uint64_t uu = -1;
  uint64_t h = 1 << 15;
  uint64_t hh = -1;

  const char *testPrintkStr = "Testing printk function!";
  printk("Printk d d test: %d %d\n", d, dd);
  printk("Printk u u test: %u %u\n", u, uu);
  printk("Printk x X x X test: %x %X %x %X\n", h, h, hh, hh);
  printk("Printk b b test: %b %b\n", h, hh);
  printk("Printk o o test: %o %o\n", h, hh);
  printk("Printk %s\n", testPrintkStr);
  printk("Printk % %% test\n");
  */
  // acpiShutdown();
  // printk("ERROR: ACPI Shutdown failed!\n");
}
