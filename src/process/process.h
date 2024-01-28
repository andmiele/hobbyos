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

#ifndef _PROCESS_H_
#define _PROCESS_H_

#define N_START_USERSPACE_PROCESSES 1

#include "../fat16/fat16.h"    // fileDescriptor
#include "../gdt/gdt.h"        // struct tss
#include "../idt/idt.h"        // struct interruptFrame
#include "../lib/lib.h"        // List
#include "../memory/memory.h"  // page size

#define STACK_SIZE PAGE_SIZE  // 4KB
#define MAX_N_PROCESSES 128
#define USER_PROGRAM_COUNTER 0x400000
#define PROC_RFLAGS \
  0x202  // set reserved bit to 1 (0x2) and enable interrupts(0x200)

#define DEFAULT_TOTAL_PROCESS_SIZE (1024 * 64)  // 16KB

#define MAX_N_FILES_PER_PROCESS 100

#define PROCESS_GUI_WINDOW_R 192
#define PROCESS_GUI_WINDOW_G 192
#define PROCESS_GUI_WINDOW_B 192

enum processState {
  PROC_UNUSED,
  PROC_INIT,
  PROC_READY,
  PROC_RUNNING,
  PROC_SLEEPING,
  PROC_KILLED
};

// ring0 process context: 6 x64 callee-save register and ring0 return address
// for ring0 process switch function

struct ring0ProcessContext {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t rbp;
  uint64_t rbx;
  uint64_t ret;
} __attribute__((packed));

enum processEvent {
  ZERO_EVENT = 1,
  PROC_EXIT_EVENT = -2,
  TIMER_WAKEUP_EVENT = -3,
  KEYBOARD_EVENT = -4
};

struct guiInfo {
  int64_t winX;                     // top-left window corner x coordinate
  int64_t winY;                     // top-left window corner y coordinate
  int64_t winWidth;                 // window width
  int64_t winHeight;                // window height
  uint64_t ownsMouse;               // process currently owns mouse pointer
  uint64_t mouseLeftButtonClicked;  // mouse left button clicked event
  char *winLabel;                   // process window label
  size_t winLabelSize;              // process window label size
  // windows RGB color
  uint8_t winR;
  uint8_t winG;
  uint8_t winB;
  uint8_t exitButtonClicked;  // exit button was clicked; exit process
};

struct process {
  struct ListNode *next;            // Pointer to next process struct
  int64_t pid;                      // Process identifier
  enum processEvent eventWaitType;  // Type of event the process is waiting for
  enum processState state;          // Process state
  uint64_t *pml4tPtr;               // Page table pointer
  uint64_t *ring0StackBasePtr;  // Kernel mode / ring0 pointer to stack area:
                                // [ring0StackPtr, ring0StackPtr + STACK_SIZE [
  struct interruptFrame *intFramePtr;  // interrupt routine frame pointer
  struct ring0ProcessContext
      *ring0ProcessContextPtr;  // Pointer to ring0ProcessContext struct
  uint64_t processTotalSize;    // Total process size (normally code + stack)
  struct fileDescriptor
      *fileDescPtrArray[MAX_N_FILES_PER_PROCESS];  // Array of file descriptor
                                                   // pointers for open files
  struct guiInfo gui;                              // GUI info
  struct process
      *nextInWindowDepthOrder;  // Pointer to next process in window depth order
};

// Initialize startup processes
void initStartupProcesses();
// Start idle process on core calling this function
void startIdleProcess();
// process functions
// process: yield
void yield();
// process: sleep until eventWaitType event occurs
void sleep(enum processEvent eventWaitType);
// wake up all processes waiting on eventWaitType event
void wakeUp(enum processEvent eventWaitType);
// Exit process
void exit();
// Clean up killed process list
void wait();
// Fork new process as copy of  current process
// Returns non-negative pid if succesful, negative value otherwise
int64_t fork(uint64_t rsp, uint64_t rbp, uint64_t rip, uint64_t rflags);
// Execute program loaded from input file
int64_t exec(struct process *proc, char *fileName);
// Handle GUI event
void processHandleGUIEvent();
#endif
