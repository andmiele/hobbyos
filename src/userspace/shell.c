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

#include <stddef.h>
#include <stdint.h>

#include "stdio.h"
#include "stdlib.h"

extern int64_t fork();
extern void pwait();
extern char readCharFromkeyboard();
extern uint64_t getMemorySize();
extern int64_t openFile(char *fileName);
extern int64_t exec(char *fileName);
extern int64_t closeFile(int64_t fileDescriptorIndex);

#define COMMAND_BUFFER_SIZE 80
#define N_COMMANDS 1

char *commandStrings[N_COMMANDS] = {"sysmem"};
size_t commandStringSizes[N_COMMANDS] = {6};

// SHELL COMMAND FUNCTIONS
void getMemorySizeCmd() {
  printf("Total system memory: %u MB\n", getMemorySize() / (1024 * 1024));
}

static void *commandFunctions[N_COMMANDS] = {(void *)getMemorySizeCmd};

static size_t readCommand(char *commandBuffer) {
  char cs[2] = {0};
  int commandStringSize = 0;
  while (1) {
    cs[0] = readCharFromkeyboard();
    if (cs[0] == '\n' || commandStringSize >= 80) {
      printf("%s", cs);
      break;
    } else if (cs[0] == '\b') {
      if (commandStringSize > 0) {
        commandBuffer[--commandStringSize] = 0;
        printf("%s", cs);
      }
    } else {
      commandBuffer[commandStringSize++] = cs[0];
      printf("%s", cs);
    }
  }

  return commandStringSize;
}

static int parseCommand(char *commandBuffer, uint64_t commandStringSize) {
  int command = -1;
  if (commandStringSize == commandStringSizes[0] &&
      memCompare(commandBuffer, commandStrings[0], commandStringSizes[0])) {
    command = 0;
  }
  return command;
}

int main() {
  char buffer[COMMAND_BUFFER_SIZE] = {0};
  uint64_t commandStringSize = 0;
  int command = 0;
  while (1) {
    printf("shell~: ");
    memset(buffer, 0, COMMAND_BUFFER_SIZE);
    commandStringSize = readCommand(buffer);
    if (commandStringSize == 0) {
      continue;
    }

    command = parseCommand(buffer, commandStringSize);

    if (command < 0) {
      // command not found, try to open and execute file named as input command
      int64_t fileDescriptorIndex = openFile(buffer);
      if (fileDescriptorIndex < 0) {
        printf("Invalid command\n");
      } else {
        int64_t status = closeFile(fileDescriptorIndex);
        if (status != 0) {
          printf("WARNING: could not close file!\n");
        }
        int64_t pid = fork();
        if (pid < 0) {
          printf("Shell error: fork failed!");
        } else {
          if (pid == 0) {  // child
            exec(buffer);
          } else {
            pwait(pid);
          }
        }
      }
    } else {
      switch (command) {
        case 0:
          ((void (*)())(commandFunctions[0]))();
          break;
        default:
          break;
      }
    }
  }
  return 0;
}
