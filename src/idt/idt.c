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

#include "idt.h"

#include "acpi/acpi.h"
#include "drivers/keyboard.h"
#include "gdt/gdt.h"
#include "io/io.h"
#include "lib/lib.h"
#include "memory/memory.h"
#include "process/process.h"
#include "stdio/stdio.h"

extern uint8_t *gLocalApicAddress;

// Assembly lock and unlock implementations for mutex
extern void spinLock(volatile uint8_t *lock);
extern void spinUnlock(volatile uint8_t *lock);

static uint64_t
    ticksArray[MAX_N_CORES_SUPPORTED];  // ticks: timer interrupts count

static struct idtEntryDescriptor idt[TOT_N_INTERRUPTS];
struct idtDescriptor idtDesc;

// array of addresses of interrupt handler functions
void (*interruptHandlerAddressArray[TOT_N_INTERRUPTS])(struct interruptFrame *);

// array of addresses of ISRs defined in idt.asm
extern uint64_t isrAddressArray[TOT_N_INTERRUPTS];

// Array flags for tracking if the syscall was interrupted and needs to run
// scheduler to switch process at the end; one per CPU core
extern uint64_t syscallRunSchedulerArray[MAX_N_CORES_SUPPORTED];

// Array flags for tracking if a syscall is running for ISRs; one per CPU core
extern uint64_t syscallRunningArray[MAX_N_CORES_SUPPORTED];

// assembly ISR and utility functions defined in idt.asm
extern void loadIDT(struct idtDescriptor *address);
extern uint64_t readCR2();  // read CR2 register containing virtual address that
                            // caused exception
extern void int20();        // timer
extern void int21();        // keyboard
extern void intFF();        // spurious
extern void noInt();

// returns core id
extern uint64_t getCoreId();  // ../kernel.asm

// Array of pointers to current running process, one per core
extern struct process *currentProcessArray[MAX_N_CORES_SUPPORTED];

// loadIDT function for Application Processors
void loadIDTAP() { loadIDT(&idtDesc); }

// interrupt handler selection function
void selectInterruptHandler(struct interruptFrame *framePtr) {
  //   printk("Interrupt Service Routine %u called\n", framePtr->intNumber);
  if (interruptHandlerAddressArray[framePtr->intNumber] != NULL)
    interruptHandlerAddressArray[framePtr->intNumber](framePtr);
  else {
    printk(
        "UNHANDLED EXCEPTION: interrupt %u, CORE %u, ring %x, errorCode %x, "
        "accessed virtual address %x, rip %x\n",
        framePtr->intNumber, framePtr->coreId, framePtr->cs & 3,
        framePtr->errorCode, readCR2(), framePtr->rip);
    if ((framePtr->cs & 0x3) !=
        0) {  // if the exception ocurred in user mode exit process
      printk("EXITING USER PROCESS %d\n",
             currentProcessArray[framePtr->coreId]->pid);
      exit();
    } else {  // unhandled exception occured in rinr0
      printk("KERNEL PANIC!\n");
      while (1) {
      }
    }
  }
}
// Timer interrupt handler
void int20Handler(struct interruptFrame *framePtr) {
  // printk("Timer Interrupt; CORE: %d\n", framePtr->coreId);
  ticksArray[framePtr->coreId]++;  // increment tick count
  wakeUp(TIMER_WAKEUP_EVENT);
  //   syscall in progress?
  if (syscallRunningArray[framePtr->coreId]) {
    // printk("Syscall interrupted %d; CORE: %d\n",
    //  syscallRunningArray[framePtr->coreId], framePtr->coreId);
    syscallRunSchedulerArray[framePtr->coreId] = 1;
  } else {
    yield();
  }
}

// Keyboard handler
void int21Handler(struct interruptFrame *framePtr) {
  keyboardISR();
  //  unsigned char c2[2] = {'\0', '\0'};
  //  c2[0] = readCharFromKeyboardQueue();
  //  if (c2[0] != 0) {
  //    printk("%s", c2);
  //  } else {
  // printk("Keyboard: unsupported or released key; CORE: %d\n",
  // framePtr->coreId);
  //  }
  /*
    printk("interrupt %u, CORE %u, ring %u, errorCode %d, "
           "accessed virtual address %x, rip %x\n",
           framePtr->intNumber, framePtr->coreId, framePtr->cs & 3,
           framePtr->errorCode, readCR2(), framePtr->rip);
  */
}

// Division by zero handler
void int0Handler(struct interruptFrame *framePtr) {
  printk("UNHANDLED EXCEPTION: Divide by zero; CORE %d\n", framePtr->coreId);
  if ((framePtr->cs & 0x3) !=
      0) {  // if the exception ocurred in user mode exit process
    printk("EXITING USER PROCESS\n");
    exit();
  } else {  // unhandled exception occured in rinr0
    printk("KERNEL PANIC!\n");
    while (1) {
    }
  }
}

// interupt gates: maskable interrupts are disabled when exeucting ISR
void setIDTDescriptor(int interruptNumber, void *address) {
  // pointer to descriptor entry in IDT
  struct idtEntryDescriptor *desc = &idt[interruptNumber];
  desc->offsetLow = (uint64_t)address & 0xFFFF;
  desc->selector = CODE_SEG_SELECTOR;

  // set Interrupt Stack Table (IST) 1-7: load ring0 rsp from ist1-7 instead of
  // rsp0
  switch (interruptNumber) {
    case NON_MASKABLE_INTERRUPT:
      desc->ISTAndReserved = POSSIBLE_CORRUPTED_RING0_STACK_IST;
      break;
    case DOUBLE_FAULT:
      desc->ISTAndReserved = POSSIBLE_CORRUPTED_RING0_STACK_IST;
      break;
    case INVALID_TSS:
      desc->ISTAndReserved = POSSIBLE_CORRUPTED_RING0_STACK_IST;
      break;
    case STACK_SEGMENT_FAULT:
      desc->ISTAndReserved = POSSIBLE_CORRUPTED_RING0_STACK_IST;
      break;
    case GENERAL_PROTECTION_FAULT:
      desc->ISTAndReserved = POSSIBLE_CORRUPTED_RING0_STACK_IST;
      break;
    default:
      desc->ISTAndReserved = 0;
  };

  desc->attributes = 0x8E;  // Present:1, DPL:00, 0, GT:0xE (interrupt gate)
  desc->offsetMid = ((uint64_t)address >> 16) & 0xFFFF;
  desc->offsetHigh = ((uint64_t)address >> 32) & 0xFFFFFFFF;
  desc->reserved = 0;
}

// initialize Interrupt Desctiptor Table
void initializeIDT() {
  memset(idt, 0, sizeof(struct idtEntryDescriptor) * TOT_N_INTERRUPTS);
  idtDesc.size = sizeof(struct idtEntryDescriptor) * TOT_N_INTERRUPTS - 1;
  idtDesc.address = (uint64_t)idt;

  for (int i = 0; i < TOT_N_INTERRUPTS; i++) {
    interruptHandlerAddressArray[i] = NULL;
  }
  interruptHandlerAddressArray[0] = int0Handler;
  interruptHandlerAddressArray[0x20 + TIMER_IRQ] = int20Handler;
  interruptHandlerAddressArray[0x20 + KEYBOARD_IRQ] = int21Handler;

  for (int i = 0; i < TOT_N_INTERRUPTS; i++) {
    setIDTDescriptor(i, (void *)isrAddressArray[i]);
  }

  // defined in idt.asm
  // setIDTDescriptor(0, isr0);
  // setIDTDescriptor(0x20 + TIMER_IRQ, int20);
  // setIDTDescriptor(0x20 + KEYBOARD_IRQ, int21);
  setIDTDescriptor(0x20 + SPURIOUS_IRQ, intFF);
  remapIRQ(TIMER_IRQ, TIMER_INTERRUPT, 0);        // sent to all CPUs
  remapIRQ(KEYBOARD_IRQ, KEYBOARD_INTERRUPT, 1);  // sent to single CPU
  remapIRQ(SPURIOUS_IRQ, SPURIOUS_INTERRUPT, 0);  // sent to all CPUs
  loadIDT(&idtDesc);
}

// return timer interrupt tick count
uint64_t getTicks() {
  uint64_t coreId = getCoreId();
  return ticksArray[coreId];
}
