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

#include "stdlib.h"

#include <stdint.h>

// Returns 1 if two buffer are equal, 0 otherwise
int memCompare(char *bufferA, char *bufferB, size_t size) {
  int equal = 1;
  for (size_t i = 0; i < size; i++) {
    if (bufferA[i] != bufferB[i]) {
      equal = 0;
      break;
    }
  }
  return equal;
}

// Copy size bytes from src to dest
void memcpy(void *dest, void *src, size_t size) {
  uint64_t *d64 = (uint64_t *)dest;
  uint64_t *s64 = (uint64_t *)src;
  uint8_t *d8 = (uint8_t *)dest;
  uint8_t *s8 = (uint8_t *)src;
  uint64_t end64 = (size / sizeof(uint64_t)) * sizeof(uint64_t);

  for (int i = 0; i < size / sizeof(uint64_t); i++) {
    d64[i] = s64[i];
  }
  // remainder, 1-3 bytes
  for (int i = end64; i < end64 + (size % sizeof(uint64_t)); i++) {
    d8[i] = s8[i];
  }
}
// Set size bytes starting at ptr to (char) c
void memset(void *ptr, int c, size_t size) {
  uint64_t *d64 = (uint64_t *)ptr;
  uint8_t *d8 = (uint8_t *)ptr;
  uint64_t c64 =
      ((uint64_t)c << 24) | ((uint64_t)c << 16) | ((uint64_t)c << 8) | c;
  uint64_t end64 = (size / sizeof(uint64_t)) * sizeof(uint64_t);

  for (int i = 0; i < size / sizeof(uint64_t); i++) {
    d64[i] = c64;
  }
  for (int i = end64; i < end64 + (size % sizeof(uint64_t)); i++) {
    d8[i] = c;
  }
}
