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

#include "stdio.h"

#include <stdarg.h>

#include "graphics/graphics.h"

#define MAX_N_DIGITS 64
#define STRING_BUFFER_SIZE 512

static uint8_t disablePrintK;
char map[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

// Disable kernel printk
void printkDisable() { disablePrintK = 1; }
// Enable kernel printk
void printkEnable() { disablePrintK = 0; }

int utoan(uint64_t number, char *buffer, int radix, int n) {
  char digitsBuffer[MAX_N_DIGITS];
  int size = 0;
  int base = 10;
  int upperCaseHex = 0;
  switch (radix) {
    case 2:
      base = 2;
      break;
    case 8:
      base = 8;
      break;
    case 16:
      base = 16;
      break;
    case 160:
      base = 16;
      upperCaseHex = 1;
      break;
    default:
      base = 10;
  }

  do {
    digitsBuffer[size] = map[number % base];
    size++;
    number /= base;
  } while (number);

  for (int i = size - 1, j = 0; i >= 0 && j < n; i--) {
    if (upperCaseHex) {
      *buffer = (digitsBuffer[i] >= 'a') ? (digitsBuffer[i] - ('a' - 'A'))
                                         : digitsBuffer[i];
    } else {
      *buffer = digitsBuffer[i];
    }
    j++;
    buffer++;
  }
  return size;
}

int utoa(uint64_t number, char *buffer, int radix) {
  char digitsBuffer[MAX_N_DIGITS];
  int size = 0;
  int base = 10;
  int upperCaseHex = 0;
  switch (radix) {
    case 2:
      base = 2;
      break;
    case 8:
      base = 8;
      break;
    case 16:
      base = 16;
      break;
    case 160:
      base = 16;
      upperCaseHex = 1;
      break;
    default:
      base = 10;
  }

  do {
    digitsBuffer[size] = map[number % base];
    size++;
    number /= base;
  } while (number);

  for (int i = size - 1; i >= 0; i--) {
    if (upperCaseHex) {
      *buffer = (digitsBuffer[i] >= 'a') ? (digitsBuffer[i] - ('a' - 'A'))
                                         : digitsBuffer[i];
    } else {
      *buffer = digitsBuffer[i];
    }
    buffer++;
  }

  *buffer = '\0';
  return (size + 1);
}

int itoa(int64_t number, char *buffer, int radix) {
  int size = 0;
  if (number < 0) {
    *buffer = '-';
    buffer++;
    size = 1;
    number = -number;
  }
  size += utoa(number, buffer, radix);
  return size;
}

int itoan(int64_t number, char *buffer, int radix, int n) {
  int size = 0;
  if (number < 0 && n > 0) {
    *buffer = '-';
    buffer++;
    size = 1;
    number = 0 - number;
    n--;
  }
  size += utoan(number, buffer, radix, n);
  return size;
}

// print format null-terminated string function
void printk(const char *formatStr, ...) {
  if (disablePrintK) {
    return;
  }
  char buffer[STRING_BUFFER_SIZE];
  size_t size = 0;
  int64_t number = 0;
  uint32_t i = 0;

  va_list args;

  va_start(args, formatStr);

  while (size < STRING_BUFFER_SIZE && formatStr[i] != '\0') {
    if (formatStr[i] != '%') {
      buffer[size] = formatStr[i];
      size++;
      i++;
    } else {
      switch (formatStr[++i]) {
        case 'd':  // Signed decimal
          number = va_arg(args, int64_t);
          size += itoan(number, buffer + size, 10, STRING_BUFFER_SIZE - size);
          break;
        case 'u':  // Unsigned decimal
          number = va_arg(args, uint64_t);
          size += utoan(number, buffer + size, 10, STRING_BUFFER_SIZE - size);
          break;
        case 'x':  // Lower case hexadecimal
          number = va_arg(args, uint64_t);
          size += utoan(number, buffer + size, 16, STRING_BUFFER_SIZE - size);
          break;
        case 'X':  // Upper case hexadecimal
          number = va_arg(args, uint64_t);
          size += utoan(number, buffer + size, 160, STRING_BUFFER_SIZE - size);
          break;
        case 'o':  // Octal
          number = va_arg(args, uint64_t);
          size += utoan(number, buffer + size, 8, STRING_BUFFER_SIZE - size);
          break;
        case 'b':  // Binary
          number = va_arg(args, uint64_t);
          size += utoan(number, buffer + size, 2, STRING_BUFFER_SIZE - size);
          break;
        case 's':  // String
          char *string = va_arg(args, char *);
          int j = 0;
          while (size < STRING_BUFFER_SIZE && string[j] != '\0') {
            buffer[size] = string[j];
            size++;
            j++;
          }
          break;
        default:
          buffer[size] = '%';
          size++;
          i--;
          break;
      }
      i++;
    }
  }
  printBuffer(buffer, size, 255, 255, 255);
  flushVideoMemory();
}
