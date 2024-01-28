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

// Syscalls

// Clean up killed process list
extern void pwait(int64_t pid);

extern int64_t openFile(char *name);
extern int64_t readFile(int64_t fileDescriptorIndex, uint8_t *fileBuffer,
                        size_t size);
extern int64_t closeFile(int64_t fileDescriptorIndex);
extern int64_t getFileSize(int64_t fileDescriptorIndex);

/*
int main() {
  uint64_t i = 0;
  while (1) {
    if ((i % 100000) == 0) {
      printf("User process 1 running: %d\n", i);
    }
    i++;
    pwait();
  }
  return 0;
}
*/

int main() {
  int64_t fileDescriptorIndex;
  size_t size;
  uint8_t fileBuffer[100] = {0};
  char *fileName = "TEST.TXT";

  fileDescriptorIndex = openFile(fileName);

  if (fileDescriptorIndex == -1) {
    printf("USER 1: openFile failed!\n");
  } else {
    size = getFileSize(fileDescriptorIndex);
    printf("USER 1: File Size %d\n", size);
    size_t bytesRead = readFile(fileDescriptorIndex, fileBuffer, 4);
    if (bytesRead != -1) {
      printf("USER 1: %s\n", fileBuffer);
    }
    bytesRead = readFile(fileDescriptorIndex, fileBuffer, size - bytesRead);
    if (bytesRead != -1) {
      printf("USER 1: %s", fileBuffer);
    }
  }
  return 0;
}
