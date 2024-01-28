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
#ifndef _KERNEL_H_
#define _KERNEL_H_

#include <stdint.h>

/*** ERROR CODES ***/
#define SUCCESS 0LL
#define ERR_MISALIGNED_ADDR -1LL
#define ERR_KERNEL_OVERLAP_VADDR -2LL
#define ERR_KERNEL_ADDR_LARGER_THAN_LIMIT -3LL
#define ERR_NEG_ADDR_RANGE -4LL
#define ERR_ALLOC_FAILED -5LL
#define ERR_PAGE_IS_ALREADY_MAPPED -6LL
#define ERR_PAGE_IS_NOT_PRESENT -7LL
#define ERR_PROCESS -8LL
#define ERR_SCHEDULER -9LL
#define ERR_FAT16 -10LL
#define ERR_VM -11LL

void printKernelError(int64_t errCode);

#define KERNEL_PANIC(errCode)                           \
  {                                                     \
    printKernelError(errCode);                          \
    printk("Kernel Panic %s:%u\n", __FILE__, __LINE__); \
    while (1) {                                         \
    }                                                   \
  }

#endif
