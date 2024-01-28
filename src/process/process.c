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

#include "process.h"

#include <stddef.h>

#include "../acpi/acpi.h"    // MAX_N_CORES_SUPPORTED
#include "../fat16/fat16.h"  // loadFile and constants
#include "../gdt/gdt.h"      // USER_CODE_SEG_SELECTOR, RING3_SELECTOR_BITS
#include "../kernel.h"       // Kernel error codes
#include "../lib/lib.h"      // memset, memcpy, List
#include "../stdio/stdio.h"  // printk

// File buffer for exec
static uint8_t
    fileBuffer[MAX_SUPPORTED_FAT16_SECTORS_PER_CLUSTER * SECTOR_SIZE];

// Array flags for tracking if a syscall is running for ISRs; one per CPU core
extern uint64_t syscallRunningArray[MAX_N_CORES_SUPPORTED];

// Array flags for tracking if the syscall was interrupted and needs to run
// scheduler to switch process at the end; one per CPU core
extern uint64_t syscallRunSchedulerArray[MAX_N_CORES_SUPPORTED];

// Assembly lock and unlock implementations for mutex
extern void spinLock(volatile uint8_t *lock);
extern void spinUnlock(volatile uint8_t *lock);

// returns core id
extern uint64_t getCoreId();  // ../kernel.asm

// number of cores
extern uint64_t acpiNCores;

// switch process // ../idt/idt.asm
void switchUserProcess(struct ring0ProcessContext **currProcRing0Context,
                       struct ring0ProcessContext *nextProcRing0Context);

// epilogue of timer ISR;
extern void returnFromTimerInterrupt();  // ../idt/idt.asm

// return from interrupt to get to ring3
extern void startUserProcess(
    struct interruptFrame *interruptFramePtr);  // ../idt/idt.asm

// Load cr3 register with page table address
extern void loadCR3(uint64_t pageTableAddr);

// Read cr3 register value
extern uint64_t readCR3();

// Array of TSSs; one per CPU core
extern struct tss tssArray[MAX_N_CORES_SUPPORTED];

// Array of ring0 syscall stack pointers
extern uint64_t *ring0SysCallStackPtrTable[MAX_N_CORES_SUPPORTED];

// Array of pointers to current running process, one per core
struct process *currentProcessArray[MAX_N_CORES_SUPPORTED];

// Ready-state process list
static struct ListHead readyProcessList;

// Waiting-state process list
static struct ListHead eventWaitProcessList;

// Killed-state process list
static struct ListHead killedProcessList;

volatile uint8_t processLock;       // lock for SMP access to critical sections
extern volatile uint8_t fat16Lock;  // lock for FAT16 shared structures

static struct process processTable[MAX_N_PROCESSES];

static int pid = 0;

// Remove process waiting for a specific event type from list
static struct ListNode *removeProcessWaitingForEventFromList(
    struct ListHead *list, int64_t eventWaitType) {
  struct ListNode *prev =
      (struct ListNode *)list;  // prev points to list->next ptr
  struct ListNode *curr = list->next;
  struct ListNode *proc = NULL;

  while (curr != NULL) {
    struct process *currProc = (struct process *)curr;

    if (currProc->eventWaitType == eventWaitType) {  // found process
      prev->next = curr->next;                       // remove from list
      proc = curr;

      // adjust tail
      if (list->next ==
          NULL) {  // list contained one process and it was removed
        list->tail = NULL;
      } else {
        if (curr->next == NULL) {  // list contained more than one process and
                                   // tail was removed
          list->tail = prev;
        }
      }

      break;
    }
    prev = curr;
    curr = curr->next;
  }

  return proc;
}

// Find unused process entry in process table, create kernel memory mappings,
// allocate stack, initalize process entry
static struct process *allocateNewProcess() {
  int64_t errCode = SUCCESS;
  struct process *proc = NULL;

  // serch for an unused process struct
  int i = 0;
  while ((i < MAX_N_PROCESSES) & (processTable[i].state != PROC_UNUSED)) {
    i++;
  }

  if (i == MAX_N_PROCESSES) {
    printk("ERROR allocateNewProcess: no unused process struct is avaiable\n");
    return NULL;
  }

  proc = &processTable[i];
  proc->state = PROC_INIT;

  // Create page table for kernel space (first 1GB of physical memory)
  // Allocate page for PML4T (Page Map Level-4 Table)

  uint64_t *pml4TPageMapPtr;

  pml4TPageMapPtr = kSetupVM();

  proc->pml4tPtr = pml4TPageMapPtr;

  // Allocate page for ring0 (syscall/interrupt) stack
  proc->ring0StackBasePtr = kAllocPage(&errCode);

  if (errCode != SUCCESS) {
    printk(
        "ERROR allocateNewProcess: kAllocPage for ring0 process stack "
        "failed\n");
    freeVM(pml4TPageMapPtr, 0);
    return NULL;
  }

  // zero out stack
  memset(proc->ring0StackBasePtr, 0, PAGE_SIZE);

  // Obtain unique pid
  proc->pid = pid;
  ++pid;

  uint64_t rsp = ((uint64_t)proc->ring0StackBasePtr) + PAGE_SIZE;

  proc->intFramePtr =
      (struct interruptFrame *)(rsp - sizeof(struct interruptFrame));

  proc->ring0ProcessContextPtr =
      (struct ring0ProcessContext *)(rsp - sizeof(struct interruptFrame) -
                                     sizeof(struct ring0ProcessContext));
  // set return address for switchUserProcess function to
  // returnFromTimerInterrupt for the very first time the process is scheduled
  proc->ring0ProcessContextPtr->ret = (uint64_t)returnFromTimerInterrupt;

  proc->intFramePtr->cs =
      USER_CODE_SEG_SELECTOR |
      RING3_SELECTOR_BITS;  // code segment: user-mode code segment selector

  proc->intFramePtr->rip =
      USER_PROGRAM_COUNTER;  // program counter (defined in process.h)

  proc->intFramePtr->ss =
      USER_DATA_SEG_SELECTOR |
      RING3_SELECTOR_BITS;  // stack segment: user-mode data segment selector
                            // This is the user-space stack pointer

  proc->intFramePtr->rflags = PROC_RFLAGS;  // defined in process.h

  return proc;
}

// The idle process runs in kernel (ring0) mode and calls the hlt instruction to
// suspend the core until the next interrupt
static void initIdleProcess() {
  spinLock(&processLock);
  for (int c = 0; c < acpiNCores; c++) {
    uint64_t i = 0;
    while ((i < MAX_N_PROCESSES) & (processTable[i].state != PROC_UNUSED)) {
      i++;
    }
    // The processTable entry for each idle process has to be equal to the core
    // id
    if (i != c) {
      printk(
          "ERROR initIdleProcess: idle process entry for core %d index "
          "cannot be "
          "different than %d\n",
          c, c);
      KERNEL_PANIC(ERR_PROCESS);
    }
    if (i >= MAX_N_PROCESSES) {
      printk("ERROR initIdleProcess: no unused process entries available\n");
      KERNEL_PANIC(ERR_PROCESS);
    }

    printk("Initializing idle process entry %u pid %u core\n", i, pid, c);
    struct process *proc = &processTable[i];
    proc->pid = pid;
    ++pid;
    proc->pml4tPtr =
        (uint64_t *)(PADDR_TO_VADDR(readCR3()));  // current kernel page table
    proc->state = PROC_READY;
  }
  spinUnlock(&processLock);
}

// Initialize startup processes
void initStartupProcesses() {
  struct process *proc = NULL;

  char processFileNameArray[N_START_USERSPACE_PROCESSES]
                           [FAT16_FILENAME_SIZE + FAT16_FILE_EXTENSION_SIZE +
                            2] = {"SHELL.BIN", "USER1.BIN", "USER2.BIN"};

  uint64_t processCodeSizeArray[N_START_USERSPACE_PROCESSES] = {
      (11 * SECTOR_SIZE), (11 * SECTOR_SIZE), (11 * SECTOR_SIZE)};

  // initialize idle process
  initIdleProcess();
  for (int pi = 0; pi < N_START_USERSPACE_PROCESSES; pi++) {
    spinLock(&processLock);
    proc = allocateNewProcess();

    if (proc == NULL) {
      spinUnlock(&processLock);
      printk("ERROR initStartupProcesses: allocateNewProcess failed\n");
      KERNEL_PANIC(ERR_PROCESS);
    }

    // File for system processes launched at startup must fit in a single FAT16
    // cluster

    int64_t errCode = loadFile(processFileNameArray[pi], fileBuffer);
    if (errCode != 0) {
      printk("ERROR initStartupProcesses: loadFile for %s failed\n",
             processFileNameArray[pi]);
      spinUnlock(&processLock);
      KERNEL_PANIC(ERR_FAT16);
    } else {
      printk("initStartupProcesses: loading %s\n", processFileNameArray[pi]);
    }

    printk("initUserSpaceVM\n");
    errCode =
        initUserSpaceVM(proc->pml4tPtr, (uint64_t *)fileBuffer,
                        processCodeSizeArray[pi], DEFAULT_TOTAL_PROCESS_SIZE);
    if (errCode != SUCCESS) {
      printk("ERROR initStartupProcesses: initUserSpaceVM failed\n");
      spinUnlock(&processLock);
      freeVM(proc->pml4tPtr, DEFAULT_TOTAL_PROCESS_SIZE);
      KERNEL_PANIC(ERR_PROCESS);
    }

    // Make sure PML4T entry has user mode flag set up as it is most likely set
    // to 0 (supervisor mode) by kSetupVM when mapping LAPIC and IOAPIC
    // addresses, which are normally < 4GB and therefore less than 512GB
    uint64_t PML4TEntryIndex =
        VADDR_TO_PML4T_INDEX((uint64_t)USER_PROGRAM_COUNTER);
    proc->pml4tPtr[PML4TEntryIndex] |= PAGE_DIRECTORY_ENTRY_U;

    proc->processTotalSize = DEFAULT_TOTAL_PROCESS_SIZE;

    proc->intFramePtr->rsp =
        USER_PROGRAM_COUNTER +
        DEFAULT_TOTAL_PROCESS_SIZE;  // set user space stack pointer to the end
                                     // of process virtual address space

    proc->state = PROC_READY;
    appendToListTail(&readyProcessList, (struct ListNode *)proc);
    spinUnlock(&processLock);
  }
}

// start idle process
void startIdleProcess() {
  uint64_t coreId = getCoreId();
  struct process *proc = &processTable[coreId];
  if (proc == NULL) {
    printk("ERROR CORE %d startProcess: NULL idle process pointer\n", coreId);
    KERNEL_PANIC(ERR_PROCESS);
  }
  proc->state = PROC_RUNNING;
  currentProcessArray[coreId] = proc;

  printk("Starting idle process %d on core %d\n", proc->pid, coreId);
}

// Run scheduler to switch process
static void schedule() {
  uint64_t coreId = getCoreId();
  struct process *currentProcess = currentProcessArray[coreId];
  struct process *nextProcess = NULL;

  if (isListEmpty(&readyProcessList)) {
    // printk("CORE %d schedule: empty Ready Process List; run idle process\n",
    //        coreId);
    if (currentProcess->pid == coreId) {
      printk("ERROR CORE %d schedule: idle process already running", coreId);
      spinUnlock(&processLock);
      KERNEL_PANIC(ERR_SCHEDULER);
    }
    nextProcess = &processTable[coreId];
  }

  else {
    nextProcess = (struct process *)removeListHead(&readyProcessList);
    // printk("CORE %d: Scheduling Process %d from %d\n", coreId,
    // nextProcess->pid,
    //        currentProcess->pid);
  }
  // Set ring0 TSS stack pointer to per process stack
  tssArray[coreId].rsp0 =
      ((uint64_t)(nextProcess->ring0StackBasePtr)) + PAGE_SIZE;
  // Set ring0 syscall stack pointer to per process stack
  ring0SysCallStackPtrTable[coreId] =
      (nextProcess->ring0StackBasePtr + PAGE_SIZE / sizeof(uint64_t));
  loadCR3((uint64_t)VADDR_TO_PADDR(nextProcess->pml4tPtr));

  nextProcess->state = PROC_RUNNING;
  currentProcessArray[coreId] = nextProcess;
  // For idle processes, the ring0 process context pointer points to an address
  // within the initial kernel stack // This function pushes the 6 x64
  // callee-saved registers onto the stack and thens sets the ring0 process
  // context pointer to rsp
  switchUserProcess(&(currentProcess->ring0ProcessContextPtr),
                    nextProcess->ring0ProcessContextPtr);
}

// Have current process yield and run scheduler
void yield() {
  uint64_t coreId = getCoreId();
  spinLock(&processLock);

  if (isListEmpty(&readyProcessList)) {
    // printk("Yield on core %d: empty Ready ProcessList, ", coreId);
    if (currentProcessArray[coreId]->pid != 0) {
      //    printk("keep running Process %d\n",
      //    currentProcessArray[coreId]->pid);
    } else {
      //     printk("keep running Idle Process (%d)\n",
      //            currentProcessArray[coreId]->pid);
    }
    spinUnlock(&processLock);
    return;
  }
  struct process *currentProcess = currentProcessArray[coreId];
  currentProcess->state = PROC_READY;

  // idle process is not added to Ready Process List
  if (currentProcess->pid != coreId) {
    appendToListTail(&readyProcessList, (struct ListNode *)currentProcess);
  }

  schedule();
}

// Put process on eventWait list
void sleep(enum processEvent eventWaitType) {
  uint64_t coreId = getCoreId();
  struct process *currentProcess = currentProcessArray[coreId];
  currentProcess->state = PROC_SLEEPING;
  currentProcess->eventWaitType = eventWaitType;
  spinLock(&processLock);
  appendToListTail(&eventWaitProcessList, (struct ListNode *)currentProcess);
  schedule();
}

// Wake up processes waiting on specific event (remove from eventWait list and
// add to ready list) from sleeping state
void wakeUp(enum processEvent eventWaitType) {
  spinLock(&processLock);
  struct process *proc = (struct process *)removeProcessWaitingForEventFromList(
      &eventWaitProcessList, eventWaitType);

  while (proc != NULL) {
    proc->state = PROC_READY;
    appendToListTail(&readyProcessList, (struct ListNode *)proc);
    proc = (struct process *)removeProcessWaitingForEventFromList(
        &eventWaitProcessList, eventWaitType);
  }
  spinUnlock(&processLock);
}

// Exit process
void exit() {
  uint64_t coreId = getCoreId();
  struct process *currentProcess = currentProcessArray[coreId];
  currentProcess->state = PROC_KILLED;
  currentProcess->eventWaitType =
      currentProcess->pid;  // set pid as eventWaitType for killed process list
                            // clean-up process
  spinLock(&processLock);
  appendToListTail(&killedProcessList, (struct ListNode *)currentProcess);
  spinUnlock(&processLock);

  wakeUp(
      PROC_EXIT_EVENT);  // wake up process that cleans up killed process list
  spinLock(&processLock);
  schedule();
}

// Clean up killed process list
void wait(int64_t pid) {
  while (1) {
    uint64_t coreId = getCoreId();
    spinLock(&processLock);
    if (!isListEmpty(&killedProcessList)) {
      // use pid as eventWaitType
      struct process *proc =
          (struct process *)removeProcessWaitingForEventFromList(
              &killedProcessList, pid);
      if (proc != NULL) {
        if (proc->state != PROC_KILLED) {
          printk(
              "ERROR CORE %d wait(): process on killed list is not in "
              "PROC_KILLED state\n",
              coreId);
          spinUnlock(&processLock);
          KERNEL_PANIC(ERR_SCHEDULER);
        }
        // free ring0 stack (1 4KB page)
        // printk("Free ring0 stack\n");
        int64_t errCode = kFreePage((uint64_t)proc->ring0StackBasePtr);
        if (errCode != SUCCESS) {
          printk(
              "ERROR CORE %d wait(), kFreePage: freeing process ring0 stack "
              "page failed\n",
              coreId);
          spinUnlock(&processLock);
          KERNEL_PANIC(errCode);
        }
        // free process page table
        freeVM(proc->pml4tPtr, proc->processTotalSize);

        // clean up File Descriptor pointer array
        for (int i = 0; i < 0; i++) {
          if (proc->fileDescPtrArray[i] != NULL) {
            spinLock(&fat16Lock);
            proc->fileDescPtrArray[i]->fileControlBlockPtr->referenceCount--;
            spinUnlock(&fat16Lock);
            proc->fileDescPtrArray[i]->nReferencingProcesses--;
            if (proc->fileDescPtrArray[i]->nReferencingProcesses ==
                0) {  // there are no processes using this File Descriptor: set
                      // File Control Block pointer to NULL to free it up
              proc->fileDescPtrArray[i]->fileControlBlockPtr = NULL;
            }
          }
        }

        // zero out struct process
        // notice: PROC_UNUSED = 0
        memset(proc, 0, sizeof(struct process));
        spinUnlock(&processLock);
        break;
      } else {
        spinUnlock(&processLock);
        sleep(PROC_EXIT_EVENT);
      }
    } else {
      spinUnlock(&processLock);
      sleep(PROC_EXIT_EVENT);
    }
  }
}

// Create new process as copy of current process
int64_t fork(uint64_t rsp, uint64_t rbp, uint64_t rip, uint64_t rflags) {
  int64_t errCode = SUCCESS;
  uint64_t coreId = getCoreId();
  struct process *currentProcess = currentProcessArray[coreId];
  struct process *newProcess = NULL;
  spinLock(&processLock);

  newProcess = allocateNewProcess();

  if (newProcess == NULL) {
    spinUnlock(&processLock);
    printk("ERROR fork: allocateNewProcess failed\n");
    return -1;
  }

  printk("fork: copyUserSpaceVM %d\n", currentProcess->processTotalSize);

  errCode = copyUserSpaceVM(newProcess->pml4tPtr, currentProcess->pml4tPtr,
                            (uint64_t *)USER_PROGRAM_COUNTER,
                            currentProcess->processTotalSize);

  if (errCode != SUCCESS) {
    printk("ERROR fork: copyUserSpaceVM failed\n");
    spinUnlock(&processLock);
    freeVM(newProcess->pml4tPtr, DEFAULT_TOTAL_PROCESS_SIZE);
    KERNEL_PANIC(ERR_PROCESS);
  }
  // Make sure PML4T entry has user mode flag set up as it is most likely set
  // to 0 (supervisor mode) by kSetupVM when mapping LAPIC and IOAPIC
  // addresses, which are normally < 4GB and therefore less than 512GB

  uint64_t PML4TEntryIndex =
      VADDR_TO_PML4T_INDEX((uint64_t)USER_PROGRAM_COUNTER);
  newProcess->pml4tPtr[PML4TEntryIndex] |= PAGE_DIRECTORY_ENTRY_U;

  newProcess->processTotalSize = currentProcess->processTotalSize;

  memcpy(newProcess->fileDescPtrArray, currentProcess->fileDescPtrArray,
         sizeof(struct fileDescriptor *) * MAX_N_FILES_PER_PROCESS);

  for (int i = 0; i < MAX_N_FILES_PER_PROCESS; i++) {
    if (currentProcess->fileDescPtrArray[i] != NULL) {
      currentProcess->fileDescPtrArray[i]->nReferencingProcesses++;
      spinLock(&fat16Lock);
      currentProcess->fileDescPtrArray[i]
          ->fileControlBlockPtr->referenceCount++;
      spinUnlock(&fat16Lock);
    }
  }

  memcpy(newProcess->intFramePtr, currentProcess->intFramePtr,
         sizeof(struct interruptFrame));

  newProcess->state = PROC_READY;

  newProcess->intFramePtr->rax =
      0;  // set this to zero to distinguish child process from parent that will
          // receive child process pid > 0 in rax as syscall return value
  newProcess->intFramePtr->rsp = rsp;
  newProcess->intFramePtr->rbp = rbp;
  newProcess->intFramePtr->rip = rip;
  newProcess->intFramePtr->rflags = rflags;

  appendToListTail(&readyProcessList, (struct ListNode *)newProcess);
  spinUnlock(&processLock);

  return newProcess->pid;
}

// Execute program loaded from input file
int64_t exec(struct process *proc, char *fileName) {
  int64_t size;
  uint64_t coreId = getCoreId();
  int64_t fileDescIndex = openFile(proc, fileName);

  if (fileDescIndex == -1) {
    printk("ERROR exec core %d: could not read file\n", coreId);
    // before calling functions that call schedule, make sure to clear
    // syscallRunningArray for current core as syscall will not be running after
    // schedule is called
    syscallRunningArray[getCoreId()] = 0;
    exit();
    // syscall is running now
    syscallRunningArray[getCoreId()] = 1;
  }

  size = getFileSize(proc, fileDescIndex);

  if (size == -1) {
    printk("ERROR exec core %d: getFileSize failed\n", coreId);
    // before calling functions that call schedule, make sure to clear
    // syscallRunningArray for current core as syscall will not be running after
    // schedule is called
    syscallRunningArray[getCoreId()] = 0;
    exit();
    // syscall is running now
    syscallRunningArray[getCoreId()] = 1;
  }

  if (size > (DEFAULT_TOTAL_PROCESS_SIZE - PAGE_SIZE)) {
    printk("ERROR exec core %d: file size can be at most %d bytes\n", coreId,
           DEFAULT_TOTAL_PROCESS_SIZE - PAGE_SIZE);
    // before calling functions that call schedule, make sure to clear
    // syscallRunningArray for current core as syscall will not be running after
    // schedule is called
    syscallRunningArray[getCoreId()] = 0;
    exit();
    // syscall is running now
    syscallRunningArray[getCoreId()] = 1;
  }

  printk("exec: loading file %s (%d bytes)\n", fileName, size);
  // Zero out process memory
  memset((void *)USER_PROGRAM_COUNTER, 0, proc->processTotalSize);

  // Read file content
  int64_t bytesRead =
      readFile(proc, fileDescIndex, (void *)USER_PROGRAM_COUNTER, size);

  if (bytesRead < 0) {
    printk("ERROR exec core %d: file content read failed\n", coreId);
    // before calling functions that call schedule, make sure to clear
    // syscallRunningArray for current core as syscall will not be running after
    // schedule is called
    syscallRunningArray[getCoreId()] = 0;
    exit();
    // syscall is running now
    syscallRunningArray[getCoreId()] = 1;
  }

  int64_t status = closeFile(proc, fileDescIndex);
  if (status != 0) {
    printk("ERROR exec core %d: file close failed\n", coreId);
    // before calling functions that call schedule, make sure to clear
    // syscallRunningArray for current core as syscall will not be running after
    // schedule is called
    syscallRunningArray[getCoreId()] = 0;
    exit();
    // syscall is running now
    syscallRunningArray[getCoreId()] = 1;
  }
  // Zero out interrupt frame
  memset(proc->intFramePtr, 0, sizeof(struct interruptFrame));

  proc->intFramePtr->cs =
      USER_CODE_SEG_SELECTOR |
      RING3_SELECTOR_BITS;  // code segment: user-mode code segment selector

  proc->intFramePtr->rip =
      USER_PROGRAM_COUNTER;  // program counter (defined in process.h)

  proc->intFramePtr->ss =
      USER_DATA_SEG_SELECTOR |
      RING3_SELECTOR_BITS;  // stack segment: user-mode data segment selector
                            // This is the user-space stack pointer

  proc->intFramePtr->rflags = PROC_RFLAGS;  // defined in process.h

  proc->intFramePtr->rsp =
      USER_PROGRAM_COUNTER +
      DEFAULT_TOTAL_PROCESS_SIZE;  // set user space stack pointer to the end
                                   // of process virtual address space
  return 0;
}
