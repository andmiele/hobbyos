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

#ifndef _AERIAL_FONT_H_
#define _AERIAL_FONT_H_

#include <stdint.h>

#define N_PRINTABLE_CHARACTERS 127
#define AERIAL_FONT_HEIGHT 15
#define AERIAL_FONT_WIDTH 10

// If c is a printable character and its glyph has a pixel set at position (x,y)
// returns 1 and returns 0 otherwise
int64_t hasPixel(char c, uint64_t x, uint64_t y);
#endif
