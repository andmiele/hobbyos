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

#include "mouse.h"

#include <stdint.h>

#include "../graphics/aerialFont.h"  // character height and width
#include "../graphics/graphics.h"    // screen resolution
#include "../idt/idt.h"
#include "../io/io.h"
#include "../process/process.h"  // processHandleGUIEvent
#include "../stdio/stdio.h"

#define MOUSE_GLYPH_WIDTH AERIAL_FONT_WIDTH
#define MOUSE_GLYPH_HEIGHT AERIAL_FONT_HEIGHT
#define PS2_MOUSE_READ_OP 0
#define PS2_MOUSE_WRITE_OP 1

#define PS2_MOUSE_OVERFLOW_Y 0b10000000
#define PS2_MOUSE_OVERFLOW_X 0b01000000
#define PS2_MOUSE_NEGATIVE_Y 0b00100000
#define PS2_MOUSE_NEGATIVE_X 0b00010000
#define PS2_MOUSE_ALWAYS_ON_BIT 0b00001000
#define PS2_MOUSE_MIDDLE_CLICK 0b00000100
#define PS2_MOUSE_RIGHT_CLICK 0b00000010
#define PS2_MOUSE_LEFT_CLICK 0b00000001

// VESA BIOS Extensions (VBE) info block pointer
extern struct VBEInfoBlock *gVBEInfoBlockPtr;

// PS2 Mouse state variables

// Global mouse pointer position
int64_t gMouseX, gMouseY;
// Global mouse movement
int64_t gMouseXMove, gMouseYMove;

static uint8_t mouseData[4];

uint64_t gLeftButtonClicked;
uint64_t gRightButtonClicked;
uint64_t gMiddleButtonClicked;
static int64_t currentMouseDataByte = 0;
// static uint64_t mouseSpeed;

// Wait for PS2 mouse to be ready for read or write command operation
static void mouseWait(uint8_t op) {
  int attempts = 100000;
  // read
  if (op == PS2_MOUSE_READ_OP) {
    while (attempts--) {
      if ((inb(PS2_COMMAND_IO_PORT) & 1) == 1) return;
    }
    return;
  } else {  // write
    while (attempts--) {
      if ((inb(PS2_COMMAND_IO_PORT) & 2) == 0) return;
    }
    return;
  }
}

static void mouseWrite(uint8_t input) {
  mouseWait(PS2_MOUSE_WRITE_OP);
  outb(PS2_COMMAND_IO_PORT, PS2_WRITE_NEXT_BYTE_TO_SECOND_PORT_CMD);
  mouseWait(PS2_MOUSE_WRITE_OP);
  outb(PS2_DATA_IO_PORT, input);
}

static uint8_t mouseRead() {
  mouseWait(PS2_MOUSE_READ_OP);
  return inb(PS2_DATA_IO_PORT);
}

// Initialize PS2 mouse
void mouseInit() {
  uint8_t status;

  gMouseX = 0;
  gMouseX = 0;
  gMouseXMove = 0;
  gMouseYMove = 0;
  gLeftButtonClicked = 0;
  gRightButtonClicked = 0;
  gMiddleButtonClicked = 0;

  // Enable second PS2 port
  mouseWait(PS2_MOUSE_WRITE_OP);
  outb(PS2_COMMAND_IO_PORT, PS2_ENABLE_SECOND_PORT_CMD);

  // Reset mouse
  mouseWait(PS2_MOUSE_WRITE_OP);
  outb(PS2_COMMAND_IO_PORT, PS2_WRITE_NEXT_BYTE_TO_SECOND_PORT_CMD);
  mouseWait(PS2_MOUSE_WRITE_OP);
  outb(PS2_DATA_IO_PORT, PS2_RESET_CMD);

  inb(PS2_DATA_IO_PORT);           // ack
  status = inb(PS2_DATA_IO_PORT);  // 0xAA on success
  if (status != 0xAA) {
    printk("ERROR mouseInit: PS2 Mouse initialization failed\n");
    printk("Kernel Panic!");
    while (1) {
    }
  }
  inb(PS2_DATA_IO_PORT);  // mouse id

  // Enable IRQ12
  mouseWait(PS2_MOUSE_WRITE_OP);
  outb(PS2_DATA_IO_PORT, PS2_READ_BYTE_0_CMD);

  mouseWait(PS2_MOUSE_READ_OP);
  status = (inb(PS2_DATA_IO_PORT) | 2);

  mouseWait(PS2_MOUSE_WRITE_OP);
  outb(PS2_COMMAND_IO_PORT, PS2_DATA_IO_PORT);

  mouseWait(PS2_MOUSE_WRITE_OP);
  outb(PS2_DATA_IO_PORT, status);

  mouseWrite(PS2_SET_DEFAULTS_CMD);  // Disables streaming, sets the packet
                                     // rate to 100 per second, and resolution
                                     // to 4 pixels per mm
  mouseRead();                       // acknowledge

  mouseWrite(
      PS2_ENABLE_SCANNING_CMD);  // The mouse starts sending automatic packets
                                 // when the mouse moves or is clicked
  mouseRead();                   // acknowledge
}

// Process PS2 mouse packet and update pointer
static void processMousePacket() {
  uint8_t status = mouseData[0];
  int32_t xMovement = mouseData[1];
  int32_t yMovement = mouseData[2];
  if (status & PS2_MOUSE_OVERFLOW_X || status & PS2_MOUSE_OVERFLOW_Y) {
    return;
  }
  if (status & PS2_MOUSE_NEGATIVE_X) {
    xMovement |= 0xffffff00;  // sign extension
  }

  if (status & PS2_MOUSE_NEGATIVE_Y) {
    yMovement |= 0xffffff00;  // sign extension
  }

  gLeftButtonClicked = status & PS2_MOUSE_LEFT_CLICK;
  gRightButtonClicked = status & PS2_MOUSE_RIGHT_CLICK;
  gMiddleButtonClicked = status & PS2_MOUSE_MIDDLE_CLICK;

  if (xMovement > 0) {
    gMouseX += N_PIXELS_MOUSE_MOVE;
    gMouseXMove = N_PIXELS_MOUSE_MOVE;
  } else if (xMovement < 0) {
    gMouseX -= N_PIXELS_MOUSE_MOVE;
    gMouseXMove = -N_PIXELS_MOUSE_MOVE;
  }

  if (yMovement > 0) {
    gMouseY -= N_PIXELS_MOUSE_MOVE;
    gMouseYMove = -N_PIXELS_MOUSE_MOVE;
  } else if (yMovement < 0) {
    gMouseY += N_PIXELS_MOUSE_MOVE;
    gMouseYMove = N_PIXELS_MOUSE_MOVE;
  }
  if (gMouseX < 0) {
    gMouseX = 0;
    gMouseXMove = 0;
  } else {
    if (gMouseX > gVBEInfoBlockPtr->xResolution - MOUSE_GLYPH_WIDTH) {
      gMouseX = gVBEInfoBlockPtr->xResolution - MOUSE_GLYPH_WIDTH;
      gMouseXMove = 0;
    }
  }
  if (gMouseY < 0) {
    gMouseY = 0;
    gMouseYMove = 0;
  } else {
    if (gMouseY > gVBEInfoBlockPtr->yResolution - MOUSE_GLYPH_HEIGHT) {
      gMouseY = gVBEInfoBlockPtr->yResolution - MOUSE_GLYPH_HEIGHT;
      gMouseYMove = 0;
    }
  }
}

// PS2 Mouse Interrupt Service Routine
void mouseISR() {
  uint8_t mouseByte = mouseRead();

  // First data byte must have PS2_MOUSE_ALWAYS_ON_BIT set
  if (currentMouseDataByte == 0 && !((mouseByte & PS2_MOUSE_ALWAYS_ON_BIT))) {
    return;
  }

  mouseData[currentMouseDataByte] = mouseByte;
  currentMouseDataByte++;

  if (currentMouseDataByte >= 3) {  // wrap around
    currentMouseDataByte = 0;
  }
  // if first byte, process packet
  if (currentMouseDataByte == 0) {
    processMousePacket();
  }
  clearScreen(64, 224, 208);
  processHandleGUIEvent();
  drawMousePointer(255, 0, 0);
  flushVideoMemory();
}
