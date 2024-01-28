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

#ifndef _LIB_H_
#define _LIB_H_

#include <stddef.h>
#include <stdint.h>

/*** Pointer List Data Structure ***/
// This struct is essentially just a pointer to the next node in the list.
// An instance of the struct needs to be declared inside the struct (or buffer)
// that one wants to insert in the list.
// This way the pointer just points to the struct instance contained in the next
// struct or buffer in the list
struct ListNode {
  struct ListNode *next;
};

struct List {
  struct ListNode *next;
  struct ListNode *tail;
};

void appendToListHead(struct List *list, struct ListNode *node);
void appendToListTail(struct List *list, struct ListNode *node);
struct ListNode *removeList(struct List *list);
int isListEmpty(const struct List *list);

/*** Memory utility functions ***/

// Set size bytes starting at ptr to (char) c
void memset(void *ptr, int c, size_t size);
// Copy size bytes from src to dest
void memcpy(void *dest, void *src, size_t size);
// Returns 1 if the first size uint8_t values in buf1 and buf2 are equal, 0
// otherwise
int bufferEqual(uint8_t *buf1, uint8_t *buf2, size_t size);

/*** String utility functions ***/

// returns length of null-terminated string
size_t strlen(const char *str);
// Copy at most n characters from dStr to sStr
// sStr must have size at least n + 1 (for terminating '\0')
size_t strncpy(char *dStr, const char *sStr, size_t n);
#endif
