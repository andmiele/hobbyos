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

#include "gui.h"

#include "graphics.h"

// Draw width * height process window using RGB color; x and y are coordinates
// of top-left corner
int64_t drawWindow(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
                   uint8_t r, uint8_t g, uint8_t b, char* winLabel,
                   size_t winLabelSize) {
  // Draw bar
  int64_t status = drawRectangle(x, y, width, WINDOW_BAR_HEIGHT, 18, 30, 19);
  if (status != 0) {  // error
    return status;
  }

  // Draw window
  status = drawRectangle(x, y + WINDOW_BAR_HEIGHT, width, height, r, g, b);
  if (status != 0) {  // error
    return status;
  }

  // Draw switch color button
  status = drawRectangle(x, y + WINDOW_BAR_HEIGHT, COLOR_BUTTON_WIDTH,
                         COLOR_BUTTON_HEIGHT, 75, 30, 19);
  if (status != 0) {  // error
    return status;
  }

  // Draw exit button
  status =
      drawCircle(x + width - EXIT_BUTTON_RADIUS - 1, y + EXIT_BUTTON_RADIUS + 1,
                 EXIT_BUTTON_RADIUS, 35, 200, 19);
  if (status != 0) {  // error
    return status;
  }

  printBufferXY("Change Color", 12, x, y + WINDOW_BAR_HEIGHT, 255, 255, 255);

  printBufferXY(
      winLabel,
      winLabelSize < MAX_WIN_LABEL_SIZE ? winLabelSize : MAX_WIN_LABEL_SIZE, x,
      y, 255, 255, 255);

  return status;
}
