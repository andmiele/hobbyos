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

#include "vga.h"

#include <stdarg.h>
#include <stddef.h>

extern void spinLock(uint8_t *lock);
extern void spinUnlock(uint8_t *lock);

// lock for multiple cores
static uint8_t vgaLock;

static uint16_t *videoMem = ((uint16_t *)VGA_MEM_PTR);
static uint16_t videoMemCursorX = 0;
static uint16_t videoMemCursorY = 0;

// VGA Text Mode: color: 8 bits | character: 8 bits
// concatenate character anc color (little-endian)
static uint16_t combineCharColorVGA(char character, char color) {
  return ((uint16_t)character | ((uint16_t)color << 8));
}

// put input character with input color at x,y coordinates
static void putCharVGA(int x, int y, char character, char color) {
  if (x < VGA_WIDTH && y < VGA_HEIGHT) {
    videoMem[y * VGA_WIDTH + x] = combineCharColorVGA(character, color);
  }
}

// put input character with input color at current x,y cursor coordinates
static void writeCharVGA(char character, char color) {
  // backspace
  if (character == '\b') {
    // top-left corner: do nothing
    if (videoMemCursorY == 0 && videoMemCursorX == 0) {
      return;
    }
    // beginning of a line, move to end of previous line
    if (videoMemCursorX == 0) {
      videoMemCursorY--;
      videoMemCursorX = VGA_WIDTH - 1;
    } else {
      videoMemCursorX--;
    }

    // clear character
    putCharVGA(videoMemCursorX, videoMemCursorY, 0, 0);
    return;
  }

  // cursor points to last character in video memory: scroll!
  if (videoMemCursorY == VGA_HEIGHT) {
    for (int y = 1; y < VGA_HEIGHT; y++) {
      for (int x = 0; x < VGA_WIDTH; x++) {
        videoMem[(y - 1) * VGA_WIDTH + x] = videoMem[(y)*VGA_WIDTH + x];
      }
    }
    // clear last row
    for (int x = 0; x < VGA_WIDTH; x++)
      videoMem[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = combineCharColorVGA(0, 0);
    videoMemCursorY = VGA_HEIGHT - 1;
  }

  // new line
  if (character == '\n') {
    videoMemCursorY++;
    videoMemCursorX = 0;
    return;
  }

  putCharVGA(videoMemCursorX, videoMemCursorY, character, color);

  if (videoMemCursorX == VGA_WIDTH - 1) {
    videoMemCursorX = 0;
    videoMemCursorY += 1;
  } else {
    ++videoMemCursorX;
  }
}

// Initialize video memory with spaces (black color); to be called by Bootstrap
// Processor(BP)
void vgaInit() {
  videoMemCursorX = 0;
  videoMemCursorY = 0;
  for (int y = 0; y < VGA_HEIGHT; y++) {
    for (int x = 0; x < VGA_WIDTH; x++) {
      putCharVGA(x, y, ' ', 0);
    }
  }

  vgaLock = 0;
}

// Print size characters contained in buffer with color
void printBufferVGA(char *buffer, size_t size, char color) {
  spinLock(&vgaLock);
  for (int i = 0; i < size; i++) writeCharVGA(buffer[i], color);
  spinUnlock(&vgaLock);
}
