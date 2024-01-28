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

#include <stdint.h>

#include "stdio.h"
#include "stdlib.h"

#define FAT16_FILENAME_SIZE 8
#define FAT16_FILE_EXTENSION_SIZE 3
#define FAT16_ENTRY_EMPTY 0x0
#define FAT16_ENTRY_DELETED 0xE5
#define FAT16_FILE_EXTENSION_SIZE 3
#define FAT16_LONG_FILE_NAME_ATTRIBUTE 0x0F
#define FAT16_LONG_VOLUME_NAME_ATTRIBUTE 0x08
#define FAT16_DIRECTORY_ATTRIBUTE_FLAG 0x10
#define FAT16_ARCHIVE_ATTRIBUTE_FLAG 0x20
#define MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES 256

// FAT 16 directory entry
struct fat16DirEntry {
  uint8_t name[FAT16_FILENAME_SIZE];       // name
  uint8_t ext[FAT16_FILE_EXTENSION_SIZE];  // extension
  uint8_t attributes;                      // attribute byte
  uint8_t reserved;                        // reserved
  uint8_t creationMs;                      // creation millisecond stamp
  uint16_t creationTime;                   // creation time
  uint16_t creationDate;                   // creation date
  uint16_t lastAccessDate;                 // last access date
  uint16_t reservedFAT32;                  // reserved for FAT32
  uint16_t modifiedTime;                   // last write time
  uint16_t modifiedDate;                   // last write date
  uint16_t startingClusterIndex;           // index of first data cluster
  uint32_t fileSize;                       // file size in bytes
} __attribute__((packed));

// syscalls
extern int64_t getRootDirEntries(struct fat16DirEntry *rootDirBuffer);

struct fat16DirEntry rootDirBuffer[MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES];

int main() {
  char name[FAT16_FILENAME_SIZE + 1] = {0};
  // Root dir buffer

  int64_t nEntries = getRootDirEntries(rootDirBuffer);

  if (nEntries > 0) {
    printf("\nNumber of root dir entries: %u\n", nEntries);
    printf("\nName          IsDirectory          File Size \n");
    printf("-------------------------------------------\n");
    for (int i = 0; i < nEntries; i++) {
      if (((rootDirBuffer[i].name[0] == FAT16_ENTRY_EMPTY) ||
           (rootDirBuffer[i].name[0] == FAT16_ENTRY_DELETED) ||
           (rootDirBuffer[i].attributes == FAT16_LONG_FILE_NAME_ATTRIBUTE) ||
           (rootDirBuffer[i].attributes & FAT16_LONG_VOLUME_NAME_ATTRIBUTE))) {
        continue;
      }

      memcpy(name, rootDirBuffer[i].name, FAT16_FILENAME_SIZE);
      if ((rootDirBuffer[i].attributes & FAT16_DIRECTORY_ATTRIBUTE_FLAG)) {
        printf("%s      YES          %u bytes\n", name,
               (uint64_t)rootDirBuffer[i].fileSize);
      } else {
        printf("%s      NO           %u bytes\n", name,
               (uint64_t)rootDirBuffer[i].fileSize);
      }
    }
  }
  return 0;
}
