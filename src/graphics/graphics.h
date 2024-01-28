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

#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include <stdint.h>

#include "../memory/memory.h"

///****VESA BIOS Extensions (VBE)****////

// VBE 0x117 mode: 1024x768x64K (16-bit color)  R : G: B -> 5 bits : 6 bits: 5
// bits

#define VBE_ERROR_X_OUT_OF_BOUNDS -1
#define VBE_ERROR_Y_OUT_OF_BOUNDS -2

#define VBE_INFO_ADDRESS (0x8000 + KERNEL_SPACE_BASE_VIRTUAL_ADDRESS)

//  Use VESA VBE 0x11B mode (1280x1024x16M) to dermine max video memory buffer
//  size
#define MAX_VBE_FRAME_BUFFER_SIZE 3932160

// VESA VBE info block structure
struct VBEInfoBlock {
  uint16_t modeAttribute;    // // deprecated, other than bit 7 that indicates
                             // support for a linear frame buffer
  uint8_t windowAAttribute;  // deprecated
  uint8_t windowBAttribute;  // deprecated
  uint16_t
      windowGranualrity;  // deprecated; originally used to compute bank numbers
  uint16_t windowSize;
  uint16_t windowASegment;
  uint16_t windowBSegment;
  uint32_t windowFuncPtr;  // deprecated; used to switch banks in Protected Mode
                           // without returning to Real Mode
  uint16_t bytesPerScanLine;  // number of bytes for horizontal line
  uint16_t xResolution;       // horizontal pixel resolution
  uint16_t yResolution;       // vertical pixel resolution
  uint8_t charXSize;
  uint8_t charYSize;
  uint8_t numberOfPlanes;
  uint8_t bitsPerPixel;   // bits per pixel
  uint8_t numberOfBanks;  // deprecated; total numbr of banks
  uint8_t memoryModel;
  uint8_t
      bankSize;  // deprecated; bank size, almost always 64 KB but can be 16 KB
  uint8_t numberOfImagePages;
  uint8_t reserved0;
  uint8_t redMaskSize;
  uint8_t redFieldPosition;
  uint8_t greenMaskSize;
  uint8_t greenFieldPosition;
  uint8_t blueMaskSize;
  uint8_t blueFieldPosition;
  uint8_t reservedMaskSize;
  uint8_t reservedFieldPosition;
  uint8_t directColorInfo;
  uint32_t frameBufferPtr;  // physical address of the linear frame buffer;
                            // write here to draw to the screen
  uint32_t offScreenMemOffset;
  uint16_t offScreenMemSize;  // size of memory in the framebuffer that is not
                              // being displayed on the screen
  uint8_t reserved1[206];
};

// Initialize VBE
void graphicsInit();

void clearScreen(uint8_t r, uint8_t g, uint8_t b);

// Draw width * height rectangle using input RGB color; x and y are coordinates
// of top-left corner
int64_t drawRectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
                      uint8_t r, uint8_t g, uint8_t b);
// Draw radius r circle using input RGB color; x and y are coordinates
// of center
int64_t drawCircle(uint64_t x, uint64_t y, uint64_t radius, uint8_t r,
                   uint8_t g, uint8_t b);

// Draw character at position x, y (top-left corner) using input RGB color; if
// the character is not printable the area is cleared
int64_t drawCharacter(char c, uint64_t x, uint64_t y, uint8_t r, uint8_t g,
                      uint8_t b);
// Draw mouse pointer at global mouseX, mouseY position using input RGB color
void drawMousePointer(uint8_t r, uint8_t g, uint8_t b);

// Print size characters contained in buffer at position x,y using RGB color
void printBufferXY(char *buffer, size_t size, uint64_t x, uint64_t y, uint8_t r,
                   uint8_t g, uint8_t b);

// For Kernel printk before GUI
// Print size characters contained in buffer at cursor position using RGB color
void printBuffer(char *buffer, size_t size, uint8_t r, uint8_t g, uint8_t b);

// Returns frame buffer size in bytes
size_t getFrameBufferSize();

// Returns frame buffer address
uint64_t getFrameBufferAddress();

// Flush video memory buffer into actual frame buffer
void flushVideoMemory();
#endif
