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

#ifndef _IO_H_
#define _IO_H_
#include <stdint.h>

uint8_t inb(uint16_t port);
uint16_t inh(uint16_t port);
uint32_t inw(uint16_t port);

void outb(uint16_t port, uint8_t value);
void outh(uint16_t port, uint16_t value);
void outw(uint16_t port, uint32_t value);

#endif
