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

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#define SPECIAL_KEY_E0_STATE_BIT (0x1)
#define SHIFT (0x2)
#define CAPS_LOCK (0x4)

#define IGNORED_SPECIAL_KEY_FIRST_SCANCODE 0xE0
// The release (break) scan code for a key is obtained by adding 0x80 to the
// press (make) scan code
#define KEY_RELEASE_FLAG 0x80

#define LEFT_SHIFT_PRESS 0x2A
#define LEFT_SHIFT_RELEASE 0xAA
#define RIGHT_SHIFT_PRESS 0x36
#define RIGHT_SHIFT_RELEASE 0xB6

#define CAPS_LOCK_PRESS 0x3A

#define PS2_KEYBOARD_IO_PORT_NUMBER 0x60

#define KEYBOARD_BUFFER_SIZE 1024
#define N_EXTENDED_ASCII_CHARS 256

struct keyboardQueue {
  // the extra element is unused; this way
  // empty :=  front == back
  // full := (back - front) %  (KEYBOARD_BUFFER_SIZE + 1) ==
  // KEYBOARD_BUFFER_SIZE
  char buffer[KEYBOARD_BUFFER_SIZE + 1];
  unsigned front;
  unsigned back;
};

// Enable PS2 keyboard; to be called after mouse initialization
void keyboardInit();

// keyboard interrupt service routine
void keyboardISR();

// read (dequeue) a character from the keyboard queue
char readFromKeyboardQueue();

#endif
