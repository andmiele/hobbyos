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

#ifndef _GUI_H_
#define _GUI_H_

#include <stddef.h>
#include <stdint.h>

#define WINDOW_BAR_HEIGHT 20
#define COLOR_BUTTON_HEIGHT 30
#define COLOR_BUTTON_WIDTH (12 * 10)
#define EXIT_BUTTON_RADIUS (WINDOW_BAR_HEIGHT / 2)
#define MAX_WIN_LABEL_SIZE 10

// Draw width * height process window using RGB color; x and y are coordinates
// of top-left corner
int64_t drawWindow(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
                   uint8_t r, uint8_t g, uint8_t bi, char* winLabel,
                   size_t winLabelSize);
#endif
