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

#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>
// Returns 1 if two buffer are equal, 0 otherwise
int memCompare(char *bufferA, char *bufferB, size_t size);
// Copy size bytes from src to dest
void memcpy(void *dest, void *src, size_t size);
// Set size bytes starting at ptr to (char) c
void memset(void *ptr, int c, size_t size);

#endif
