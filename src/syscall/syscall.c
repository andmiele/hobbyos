
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

#include "syscall.h"

#include "../acpi/acpi.h"        // MAX_N_CORES_SUPPORTED
#include "../fat16/fat16.h"      // Filesystem
#include "../gdt/gdt.h"          // TSS
#include "../idt/idt.h"          // getTicks
#include "../kernel.h"           // SUCCESS
#include "../lib/lib.h"          // memset
#include "../memory/memory.h"    // kAllocPage, getMemorySize
#include "../process/process.h"  // sleep
#include "../stdio/stdio.h"      // printk
#include "../vga/vga.h"          // printBuffer
#include "drivers/keyboard.h"    // readFromKeyboardQueue

void printRsp(uint64_t rsp) { printk("RSP %x\n", rsp); }

// Array of TSSs; one per CPU core; ../gdt/gdt.c
extern struct tss tssArray[MAX_N_CORES_SUPPORTED];

// Number of active cores, computed in acpi.c
extern uint64_t gActiveCpuCount;

// Enable x64 syscall
extern void enableSysCall();  // syscall.asm

const uint64_t nSysCalls = N_SYSCALLS;

// Array flags for tracking if the syscall was interrupted and needs to run
// scheduler to switch process at the end; one per CPU core
uint64_t syscallRunSchedulerArray[MAX_N_CORES_SUPPORTED];

// Array flags for tracking if a syscall is running for ISRs; one per CPU core
uint64_t syscallRunningArray[MAX_N_CORES_SUPPORTED];

// Returns core id
extern uint64_t getCoreId();  // ../kernel.asm

// Array of pointers to current running process, one per core
extern struct process *currentProcessArray[MAX_N_CORES_SUPPORTED];

static uint64_t sysPrintBuffer(char *buffer, size_t size, char color) {
  printBuffer(buffer, size, color);
  return size;
}

// Sleep for nTicks ticks (timer interrupts)
static uint64_t sysSleep(uint64_t sleepTicks) {
  uint64_t previousTicks = getTicks();
  uint64_t currentTicks = previousTicks;

  while (currentTicks - previousTicks < sleepTicks) {
    // before calling functions that call schedule, make sure to clear
    // syscallRunningArray for current core as syscall will not be running after
    // schedule is called
    syscallRunningArray[getCoreId()] = 0;
    sleep(TIMER_WAKEUP_EVENT);
    // syscall is running now
    syscallRunningArray[getCoreId()] = 1;
    currentTicks = getTicks();
  }
  return 0;
}

// Exit process
static uint64_t sysExit() {
  // before calling functions that call schedule, make sure to clear
  // syscallRunningArray for current core as syscall will not be running after
  // schedule is called
  syscallRunningArray[getCoreId()] = 0;
  exit();
  // syscall is running now
  syscallRunningArray[getCoreId()] = 1;
  return 0;
}

// Process wait: remove process with given pid from killed process list
// (clean-up)
static uint64_t sysWait(int64_t pid) {
  // before calling functions that call schedule, make sure to clear
  // syscallRunningArray for current core as syscall will not be running after
  // schedule is called
  syscallRunningArray[getCoreId()] = 0;
  wait(pid);
  // syscall is running now
  syscallRunningArray[getCoreId()] = 1;
  return 0;
}

// Read a character from the keyboard queue
// readFromKeyboardQueue defined in drivers/keyboard.c
static char sysReadCharFromKeyboardQueue() { return readFromKeyboardQueue(); }

// Return total size of system  physical memory
static uint64_t sysGetMemorySize() { return getMemorySize(); }

// Open file given input name and return file descriptor index
static int64_t sysOpenFile(char *name) {
  return openFile(currentProcessArray[getCoreId()], name);
}

// Read size bytes from last accessed (read or write) position in the file given
// input file descriptor index and return number of bytes read
static int64_t sysReadFile(int64_t fileDescriptorIndex, uint8_t *fileBuffer,
                           size_t size) {
  return readFile(currentProcessArray[getCoreId()], fileDescriptorIndex,
                  fileBuffer, size);
}

// Close file given input file descriptor index
static int64_t sysCloseFile(int64_t fileDescriptorIndex) {
  return closeFile(currentProcessArray[getCoreId()], fileDescriptorIndex);
}

// Get file size given input file descriptor index
static int64_t sysGetFileSize(int64_t fileDescriptorIndex) {
  return getFileSize(currentProcessArray[getCoreId()], fileDescriptorIndex);
}

// Fork new process as a copy of current process
static int64_t sysFork(uint64_t rsp, uint64_t rbp, uint64_t rip,
                       uint64_t rflags) {
  int64_t pid = fork(rsp, rbp, rip, rflags);
  return pid;
}

// Execute program from input file
static int64_t sysExec(char *fileName) {
  return exec(currentProcessArray[getCoreId()], fileName);
}

// Copies FAT16 root directory entries into input buffer and returns number of
// entries
static int64_t sysGetRootDirectory(struct fat16DirEntry *rootDirEntryBuffer) {
  return getRootDirectory(rootDirEntryBuffer);
}

// Array of TSSs; one per CPU core
uint64_t *ring0SysCallStackPtrTable[MAX_N_CORES_SUPPORTED];

void *systemCallTable[N_SYSCALLS] = {(void *)sysPrintBuffer,
                                     (void *)sysSleep,
                                     (void *)sysExit,
                                     (void *)sysWait,
                                     (void *)sysReadCharFromKeyboardQueue,
                                     (void *)sysGetMemorySize,
                                     (void *)sysOpenFile,
                                     (void *)sysReadFile,
                                     (void *)sysCloseFile,
                                     (void *)sysGetFileSize,
                                     (void *)sysFork,
                                     (void *)sysExec,
                                     (void *)sysGetRootDirectory};

void initSystemCalls() {
  // Set per-core ring0 syscall stack to tss rsp0 (ring0 interrupt) stack
  // pointer for now
  // This will be overwritten by per process ring0 stack when a
  // process is run on the core
  memset(ring0SysCallStackPtrTable, 0,
         MAX_N_CORES_SUPPORTED * sizeof(uint64_t *));

  for (int i = 0; i < gActiveCpuCount; i++) {
    ring0SysCallStackPtrTable[i] = (uint64_t *)tssArray[i].rsp0;
  }

  // enable x64 syscall
  enableSysCall();
}
