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

#include "graphics.h"

#include "../lib/lib.h"  // memcpy
#include "../stdio/stdio.h"
#include "aerialFont.h"

///****VESA BIOS Extensions (VBE)****////

// Global mouse pointer position
extern int64_t gMouseX, gMouseY;

extern void spinLock(uint8_t *lock);
extern void spinUnlock(uint8_t *lock);

static uint8_t graphicsLock;

static uint16_t videoMemCursorX = 0;
static uint16_t videoMemCursorY = 0;

#define MOUSE_POINTER_GLYPH_SIZE 5
static uint16_t mousePointerGlyph[MOUSE_POINTER_GLYPH_SIZE] = {
    0b00100, 0b00100, 0b11111, 0b00100, 0b00100};

uint8_t videoMemoryBuffer[MAX_VBE_FRAME_BUFFER_SIZE];

// VESA BIOS Extensions (VBE) info block pointer
struct VBEInfoBlock *gVBEInfoBlockPtr;

// VBE 0x117 mode: 1024x768x64K (16-bit color)  R : G: B -> 5 bits : 6 bits: 5
// bits

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t scaledR = r >> 3;  // from 8 bits to 5 bits
  uint16_t scaledG = g >> 2;  // from 8 bits to 6 bits
  uint16_t scaledB = b >> 3;  // from 8 bits to 5 bits
  return ((scaledR << 11) | (scaledG << 5) | scaledB);
}

// Clear screen with input RGB color
void clearScreen(uint8_t r, uint8_t g, uint8_t b) {
  spinLock(&graphicsLock);
  uint16_t *vmPtr = (uint16_t *)videoMemoryBuffer;
  for (int i = 0;
       i < gVBEInfoBlockPtr->xResolution * gVBEInfoBlockPtr->yResolution; i++) {
    vmPtr[i] = rgb(r, g, b);
  }
  spinUnlock(&graphicsLock);
}

// Initialize VBE, to be called by BSP
void graphicsInit() {
  graphicsLock = 0;
  gVBEInfoBlockPtr = (struct VBEInfoBlock *)VBE_INFO_ADDRESS;
  printkDisable();  // stdio.h
  clearScreen(64, 224, 208);
  flushVideoMemory();
}

// Draw Pixel
static void drawPixel(uint64_t x, uint64_t y, uint8_t r, uint8_t g, uint8_t b) {
  uint16_t *vmPtr = (uint16_t *)videoMemoryBuffer;
  uint64_t index = y * gVBEInfoBlockPtr->xResolution + x;
  vmPtr[index] = rgb(r, g, b);
}

// Draw radius r circle using input RGB color; x and y are coordinates
// of center
int64_t drawCircle(uint64_t x, uint64_t y, uint64_t radius, uint8_t r,
                   uint8_t g, uint8_t b) {
  if (((int64_t)x) - ((int64_t)radius) < 0) {
    return VBE_ERROR_X_OUT_OF_BOUNDS;
  }

  if (x + radius >= gVBEInfoBlockPtr->xResolution) {
    return VBE_ERROR_X_OUT_OF_BOUNDS;
  }
  if (y + radius >= gVBEInfoBlockPtr->yResolution) {
    return VBE_ERROR_Y_OUT_OF_BOUNDS;
  }
  if (((int64_t)y) - ((int64_t)radius) < 0) {
    return VBE_ERROR_Y_OUT_OF_BOUNDS;
  }

  spinLock(&graphicsLock);
  for (int64_t j = -radius; j < (int64_t)radius; j++) {
    for (int64_t i = -radius; i < (int64_t)radius; i++) {
      if (i * i + j * j <= (int64_t)(radius * radius)) {
        drawPixel(((int64_t)x) + i, ((int64_t)y) + j, r, g, b);
      }
    }
  }
  spinUnlock(&graphicsLock);
  return 0;
}

// Draw width * height rectangle using input RGB color; x and y are coordinates
// of top-left corner
int64_t drawRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
                      uint8_t r, uint8_t g, uint8_t b) {
  if (x + width >= gVBEInfoBlockPtr->xResolution) {
    return VBE_ERROR_X_OUT_OF_BOUNDS;
  }
  if (y + height >= gVBEInfoBlockPtr->yResolution) {
    return VBE_ERROR_Y_OUT_OF_BOUNDS;
  }
  spinLock(&graphicsLock);
  for (int i = x; i < x + width; i++) {
    for (int j = y; j < y + height; j++) {
      drawPixel(i, j, r, g, b);
    }
  }
  spinUnlock(&graphicsLock);
  return 0;
}

// Put character at x, y postion using input RGB color; if the character is
// not printable the area is cleared
static void putCharacterXY(char c, uint64_t x, uint64_t y, uint8_t r, uint8_t g,
                           uint8_t b) {
  for (int i = 0; i < AERIAL_FONT_HEIGHT; i++) {
    for (int j = 0; j < AERIAL_FONT_WIDTH; j++) {
      if (hasPixel(c, j, i)) {
        drawPixel(x + j, y + i, r, g, b);
      } else {
        // drawPixel(x + j, y + i, 0, 0, 0);
      }
    }
  }
}

// For Kernel printk before GUI
// Put character at cursor postion using input RGB color; if the character is
// not printable the area is cleared
static void putCharacter(char c, uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < AERIAL_FONT_HEIGHT; i++) {
    for (int j = 0; j < AERIAL_FONT_WIDTH; j++) {
      if (hasPixel(c, j, i)) {
        drawPixel(videoMemCursorX + j, videoMemCursorY + i, r, g, b);
      } else {
        drawPixel(videoMemCursorX + j, videoMemCursorY + i, 0, 0, 0);
      }
    }
  }
}

// Draw mouse pointer at global gMouseX, gMouseY position using input RGB color
void drawMousePointer(uint8_t r, uint8_t g, uint8_t b) {
  spinLock(&graphicsLock);
  // Draw mouse cursor
  for (int y = 0; y < MOUSE_POINTER_GLYPH_SIZE; y++) {
    for (int x = 0; x < MOUSE_POINTER_GLYPH_SIZE; x++) {
      if ((mousePointerGlyph[y] >> (MOUSE_POINTER_GLYPH_SIZE - 1 - x)) & 0x1) {
        drawPixel(gMouseX + x, gMouseY + y, r, g, b);
      } else {
        // drawPixel(gMouseX + x, gMouseY + y, 0, 0, 0);
      }
    }
  }
  spinUnlock(&graphicsLock);
}

// Draw character at position x, y (top-left corner) using input RGB color; if
// the character is not printable the area is cleared
int64_t drawCharacter(char c, uint64_t x, uint64_t y, uint8_t r, uint8_t g,
                      uint8_t b) {
  if (x + AERIAL_FONT_WIDTH >= gVBEInfoBlockPtr->xResolution) {
    return VBE_ERROR_X_OUT_OF_BOUNDS;
  }

  if (y + AERIAL_FONT_HEIGHT >= gVBEInfoBlockPtr->yResolution) {
    return VBE_ERROR_Y_OUT_OF_BOUNDS;
  }
  spinLock(&graphicsLock);
  for (int i = 0; i < AERIAL_FONT_HEIGHT; i++) {
    for (int j = 0; j < AERIAL_FONT_WIDTH; j++) {
      if (hasPixel(c, j, i)) {
        drawPixel(x + j, y + i, r, g, b);
      } else {
        drawPixel(x + j, y + i, 0, 0, 0);
      }
    }
  }
  spinUnlock(&graphicsLock);
  return 0;
}

// Put input character at x,y position using
// RGB color
static void writeCharVBEXY(char character, uint64_t *x, uint64_t *y, uint8_t r,
                           uint8_t g, uint8_t b) {
  uint64_t charsPerLine = gVBEInfoBlockPtr->xResolution / AERIAL_FONT_WIDTH;

  // backspace
  if (character == '\b') {
    // top-left corner: do nothing
    if (*y == 0 && *x == 0) {
      return;
    }
    // beginning of a line, move to end of previous line
    if (*x == 0) {
      *y -= AERIAL_FONT_HEIGHT;
      *x = (charsPerLine - 1) * AERIAL_FONT_WIDTH;
    } else {
      *x -= AERIAL_FONT_WIDTH;
    }

    // clear character
    putCharacterXY(0, *x, *y, 0, 0, 0);
    return;
  }

  // new line
  if (character == '\n') {
    *y += AERIAL_FONT_HEIGHT;
    *x = 0;
    return;
  }

  putCharacterXY(character, *x, *y, r, g, b);

  if (*x + AERIAL_FONT_WIDTH >
      gVBEInfoBlockPtr->xResolution - AERIAL_FONT_WIDTH) {
    *x = 0;
    *y += AERIAL_FONT_HEIGHT;
  } else {
    *x += AERIAL_FONT_WIDTH;
  }
}

// For Kernel printk before GUI
// Put input character at current x,y cursor coordinates using
// RGB color
static void writeCharVBE(char character, uint8_t r, uint8_t g, uint8_t b) {
  uint16_t *vmPtr = (uint16_t *)videoMemoryBuffer;
  uint64_t charsPerLine = gVBEInfoBlockPtr->xResolution / AERIAL_FONT_WIDTH;
  uint64_t nLines = gVBEInfoBlockPtr->yResolution / AERIAL_FONT_HEIGHT;

  // backspace
  if (character == '\b') {
    // top-left corner: do nothing
    if (videoMemCursorY == 0 && videoMemCursorX == 0) {
      return;
    }
    // beginning of a line, move to end of previous line
    if (videoMemCursorX == 0) {
      videoMemCursorY -= AERIAL_FONT_HEIGHT;
      videoMemCursorX = (charsPerLine - 1) * AERIAL_FONT_WIDTH;
    } else {
      videoMemCursorX -= AERIAL_FONT_WIDTH;
    }

    // clear character
    putCharacter(0, 0, 0, 0);
    return;
  }

  // cursor points to last character in video memory: scroll!
  if (videoMemCursorY >= nLines * AERIAL_FONT_HEIGHT) {
    for (int y = AERIAL_FONT_HEIGHT; y < gVBEInfoBlockPtr->yResolution; y++) {
      for (int x = 0; x < gVBEInfoBlockPtr->xResolution; x++) {
        vmPtr[(y - AERIAL_FONT_HEIGHT) * gVBEInfoBlockPtr->xResolution + x] =
            vmPtr[(y)*gVBEInfoBlockPtr->xResolution + x];
      }
    }
    // clear last row
    for (int y = 0; y < AERIAL_FONT_HEIGHT; y++) {
      for (int x = 0; x < gVBEInfoBlockPtr->xResolution; x++) {
        vmPtr[(videoMemCursorY - AERIAL_FONT_HEIGHT + y) *
                  gVBEInfoBlockPtr->xResolution +
              x] = rgb(0, 0, 0);
      }
    }
    videoMemCursorY = (nLines - 1) * AERIAL_FONT_HEIGHT;
  }

  // new line
  if (character == '\n') {
    videoMemCursorY += AERIAL_FONT_HEIGHT;
    videoMemCursorX = 0;
    return;
  }

  putCharacter(character, r, g, b);

  if (videoMemCursorX + AERIAL_FONT_WIDTH >
      gVBEInfoBlockPtr->xResolution - AERIAL_FONT_WIDTH) {
    videoMemCursorX = 0;
    videoMemCursorY += AERIAL_FONT_HEIGHT;
  } else {
    videoMemCursorX += AERIAL_FONT_WIDTH;
  }
}

// For Kernel printk before GUI
// Print size characters contained in buffer using RGB color
// at current cursor position
void printBuffer(char *buffer, size_t size, uint8_t r, uint8_t g, uint8_t b) {
  spinLock(&graphicsLock);

  for (int i = 0; i < size; i++) {
    writeCharVBE(buffer[i], r, g, b);
  }
  spinUnlock(&graphicsLock);
}

// Print size characters contained in buffer at position x,y using RGB color
void printBufferXY(char *buffer, size_t size, uint64_t x, uint64_t y, uint8_t r,
                   uint8_t g, uint8_t b) {
  spinLock(&graphicsLock);

  uint64_t xCopy = x;
  uint64_t yCopy = y;

  for (int i = 0; i < size; i++) {
    writeCharVBEXY(buffer[i], &xCopy, &yCopy, r, g, b);
  }
  spinUnlock(&graphicsLock);
}

// Flush video memory buffer into actual frame buffer
void flushVideoMemory() {
  spinLock(&graphicsLock);
  memcpy((void *)(uint64_t)gVBEInfoBlockPtr->frameBufferPtr, videoMemoryBuffer,
         gVBEInfoBlockPtr->xResolution * gVBEInfoBlockPtr->yResolution *
             gVBEInfoBlockPtr->bitsPerPixel / 8);
  spinUnlock(&graphicsLock);
}

// Returns frame buffer size in bytes
size_t getFrameBufferSize() {
  return gVBEInfoBlockPtr->bytesPerScanLine * gVBEInfoBlockPtr->yResolution;
}
// Returns frame buffer address
uint64_t getFrameBufferAddress() { return gVBEInfoBlockPtr->frameBufferPtr; }
