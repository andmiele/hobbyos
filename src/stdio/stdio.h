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

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdint.h>

// Print size characters from buffer
void printBuffer(char *buffer, size_t size, char color);

// Print format string
// supported specifiers: %d (signed integer), %u (unsigned integer),
// %x (lower case hexadecimal integer), %X (upper case hexadecimal integer),
// %o (octal integer), %b (binary integer), %s (string)
void printk(const char *formatStr, ...);

// Convert signed integer to string using given radix
// write string to buffer
// returns string size
int itoa(int64_t number, char *buffer, int radix);

// Convert unsigned integer to string using given radix
// write string to buffer
// returns string size
int utoa(uint64_t number, char *buffer, int radix);

#endif
