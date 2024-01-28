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

#include "lib.h"

/*** Pointer List Data Structure ***/
int isListEmpty(const struct List *list) { return (list->next == NULL); }

void appendToListTail(struct List *list, struct ListNode *node) {
  node->next = NULL;
  if (isListEmpty(list)) {
    list->next = node;
    list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }
}

void appendToListHead(struct List *list, struct ListNode *node) {
  node->next = list->next;
  list->next = node;
  if (isListEmpty(list)) {
    list->tail = node;
  }
}

struct ListNode *removeList(struct List *list) {
  struct ListNode *node = NULL;

  if (isListEmpty(list)) {
    return NULL;
  }

  node = list->next;
  list->next = node->next;

  // there was only one node in then list. It is empty now
  if (list->next == NULL) {
    list->tail = NULL;
  }
  return node;
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

// Returns length of null-terminated string
size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len]) {
    len++;
  }
  return len;
}

// Copy at most n characters from dStr to sStr
// sStr must have size at least n + 1 (for terminating '\0')
size_t strncpy(char *dStr, const char *sStr, size_t n) {
  size_t i = 0;

  while ((i < n) && (sStr[i] != '\0')) {
    dStr[i] = sStr[i];
    ++i;
  }
  size_t nCopied = i;
  while (i < n) {
    dStr[i] = '\0';
    ++i;
  }

  return nCopied;
}

// Returns 1 if the first size uint8_t values in buf1 and buf2 are equal, 0
// otherwise
int bufferEqual(uint8_t *buf1, uint8_t *buf2, size_t size) {
  for (int i = 0; i < size; i++) {
    if (buf1[i] != buf2[i]) {
      return 0;
    }
  }
  return 1;
}
