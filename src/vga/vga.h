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

#ifndef _VGA_H_
#define _VGA_H_

#include <stdint.h>

#include "../memory/memory.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 20
#define VGA_MEM_PTR (0xb8000 + KERNEL_SPACE_BASE_VIRTUAL_ADDRESS)
#define VGA_COLOR_WHITE 15

// Print size characters contained in buffer with color
void printBuffer(char *buffer, size_t size, char color);
// Initialize video memory with spaces (black color)
void vgaInit();
#endif
