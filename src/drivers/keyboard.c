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

#include "keyboard.h"

#include <stdint.h>

#include "../acpi/acpi.h"        // MAX_N_CORES_SUPPORTED
#include "../idt/idt.h"          // PS2 constants
#include "../io/io.h"            // inb
#include "../process/process.h"  // sleep, wakeUp
#include "../stdio/stdio.h"      // printk

// Currently the keyboard driver only supports
// The PS2 Scan Code Set 1 and only the shift and caps lock special keys
// All special keys that send two scan codes for both press (make) and release
// (break) are not supported The first scan code sent by the controller for
// these special keys is 0xE0 The driver implements a simple state machine

// Scan code to ASCII map for shift off case
// 0x01 is ESC key
static char keyboardMap[N_EXTENDED_ASCII_CHARS] = {
    0,   0,   '1',  '2', '3',  '4', '5', '6',  '7', '8', '9', '0',
    '-', '=', '\b', 0,   'q',  'w', 'e', 'r',  't', 'y', 'u', 'i',
    'o', 'p', '[',  ']', '\n', 0,   'a', 's',  'd', 'f', 'g', 'h',
    'j', 'k', 'l',  ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm',  ',', '.',  '/', 0,   '*',  0,   ' '};

// Scan code to ASCII map for shift on case
static char keyboardShiftMap[N_EXTENDED_ASCII_CHARS] = {
    0,   1,   '!',  '@',  '#',  '$', '%', '^', '&', '*', '(', ')',
    '_', '+', '\b', '\t', 'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{',  '}',  '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H',
    'J', 'K', 'L',  ':',  '"',  '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M',  '<',  '>',  '?', 0,   '*', 0,   ' '};

// state machine
enum state { RESET, SPECIAL_KEY_FIRST_E0, VALID_KEY };
static uint32_t state;

// SHIFT and CAPS LOCK on flags
static uint8_t leftShiftOn;
static uint8_t rightShiftOn;
static uint8_t capsLockOn;

// Keyboard character queue data structure
// can contain up to KEYBOARD_BUFFER_SIZE characters
static struct keyboardQueue queue = {{0}, 0, 0};

// Assembly lock and unlock implementations for mutex
extern void spinLock(volatile uint8_t *lock);
extern void spinUnlock(volatile uint8_t *lock);

extern uint64_t getCoreId();

// Array flags for tracking if a syscall is running for ISRs; one per CPU core
extern uint64_t syscallRunningArray[MAX_N_CORES_SUPPORTED];

static volatile uint8_t keyboardQueueLock;

static int isKeyboardQueueFull() {
  return ((queue.back - queue.front) % (KEYBOARD_BUFFER_SIZE + 1) ==
          KEYBOARD_BUFFER_SIZE);
}

static int isKeyboardQueueEmpty() { return (queue.back == queue.front); }

static int writeToKeyboardQueue(char c) {
  spinLock(&keyboardQueueLock);

  if (isKeyboardQueueFull()) {
    spinUnlock(&keyboardQueueLock);
    return -1;
  }

  queue.buffer[queue.back] = c;
  queue.back = (queue.back + 1) % (KEYBOARD_BUFFER_SIZE + 1);
  spinUnlock(&keyboardQueueLock);
  return 0;
}

// read (dequeue) a character from the keyboard queue
char readFromKeyboardQueue() {
  char c = 0;
  spinLock(&keyboardQueueLock);
  while (isKeyboardQueueEmpty()) {  // if keyboard is empty sleep until a
                                    // character is added to the queue
    spinUnlock(&keyboardQueueLock);
    // before calling functions that call schedule, make sure to clear
    // syscallRunningArray for current core as syscall will not be running after
    // schedule is called
    syscallRunningArray[getCoreId()] = 0;
    sleep(KEYBOARD_EVENT);
    // syscall is running now
    syscallRunningArray[getCoreId()] = 1;
    spinLock(&keyboardQueueLock);
  }
  c = queue.buffer[queue.front];
  queue.front = (queue.front + 1) % (KEYBOARD_BUFFER_SIZE + 1);
  spinUnlock(&keyboardQueueLock);
  return c;
}

// implement PS2 keyboard driver state machine
// read keyboard scan code from port 0x60
// and map it to character using Scan Code Set 1
static char readCharFromKeyboard() {
  unsigned char scanCode;
  char character;

  scanCode = inb(PS2_KEYBOARD_IO_PORT_NUMBER);

  // if the scan code is equal to 0xE0 a special key was pressed or released
  // 0xE0 is the first of two scan codes sent for special keys
  if (scanCode == IGNORED_SPECIAL_KEY_FIRST_SCANCODE) {
    // set special key bit in state
    state = SPECIAL_KEY_FIRST_E0;
    return 0;
  }

  // if the first special key scan code (0xE0) was received previously, ignore
  // the second scan code and clear special key bit in state special keys are
  // not supported
  if (state == SPECIAL_KEY_FIRST_E0) {
    state = RESET;
    return 0;
  }

  // The release (break) scan code for a key is equal to the press (make) scan
  // code plus 0x80
  if (scanCode & KEY_RELEASE_FLAG) {
    state = RESET;
    // if LEFT SHIFT was released
    if (scanCode == LEFT_SHIFT_RELEASE) {
      leftShiftOn = 0;
    }

    // if RIGHTT SHIFT was released
    if (scanCode == RIGHT_SHIFT_RELEASE) {
      rightShiftOn = 0;
    }
    return 0;
  }

  state = VALID_KEY;

  // if LEFT SHIFT was pressed
  if (scanCode == LEFT_SHIFT_PRESS) {
    leftShiftOn = 1;
    return 0;
  }

  // if RIGHT SHIFT was pressed
  if (scanCode == RIGHT_SHIFT_PRESS) {
    rightShiftOn = 1;
    return 0;
  }

  // if CAPS LOCK  was pressed (we only care about caps lock being pressed:
  // first time to activate, second time to deactivate)
  if (scanCode == CAPS_LOCK_PRESS) {
    capsLockOn ^= 1;  // disable CAPS LOCK if it was on; enable otherwise
    return 0;
  }

  // if SHIFT is on
  if (leftShiftOn || rightShiftOn) {
    character = keyboardShiftMap[scanCode];
  } else {  // else if SHIFT is off
    character = keyboardMap[scanCode];
  }

  // if CAPS LOCK is on, switch case if character is a letter
  if (capsLockOn) {
    if (character >= 'a' && character <= 'z') {  // capitalize
      character -= 32;
    } else if (character >= 'A' && character <= 'Z') {  // decapitalize
      character += 32;
    }
  }

  return character;
}

// Keyboard Interrupt Service Routine
void keyboardISR() {
  char c = readCharFromKeyboard();
  if (c > 0) {
    writeToKeyboardQueue(c);
    // wake up process waiting for keyboard
    wakeUp(KEYBOARD_EVENT);
  }
}

// Enable PS2 keyboard; to be called after mouse initialization
void keyboardInit() {
  outb(PS2_COMMAND_IO_PORT, PS2_DISABLE_FIRST_PORT_CMD);
  outb(PS2_COMMAND_IO_PORT,
       PS2_DISABLE_SECOND_PORT_CMD);  // ignored if not supported
  // flush device buffer
  inb(PS2_DATA_IO_PORT);
  outb(PS2_COMMAND_IO_PORT, PS2_READ_BYTE_0_CMD);
  uint8_t status = inb(PS2_DATA_IO_PORT);
  uint8_t secondCh =
      (status & 0b00100000) == 0b00100000;  // if 0 no second channel
  if (secondCh == 0) {
    printk("Keyboard initialization: there is no second channel\n");
  }
  status |= 0b01000011;  //...0x11 enabled irqs for both ports
  outb(PS2_COMMAND_IO_PORT, PS2_WRITE_NEXT_BYTE_0_CMD);
  outb(PS2_DATA_IO_PORT, status);
  // finsh PS/2 initalization by reenabling devices
  outb(PS2_COMMAND_IO_PORT, PS2_ENABLE_FIRST_PORT_CMD);
  outb(PS2_COMMAND_IO_PORT,
       PS2_ENABLE_SECOND_PORT_CMD);  // ignored if unsupported
  outb(PS2_COMMAND_IO_PORT, PS2_RESET_CMD);
}
