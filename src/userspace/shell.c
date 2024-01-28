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


// Launch new processes/windows by typing p and then enter!

char *commandStrings[N_COMMANDS] = {"p"};
size_t commandStringSizes[N_COMMANDS] = {1};

static size_t readCommand(char *commandBuffer) {
  char cs[2] = {0};
  int commandStringSize = 0;
  while (1) {
    cs[0] = readCharFromkeyboard();
    if (cs[0] == '\n' || commandStringSize >= 80) {
      break;
    } else if (cs[0] == '\b') {
      if (commandStringSize > 0) {
        commandBuffer[--commandStringSize] = 0;
      }
    } else {
      commandBuffer[commandStringSize++] = cs[0];
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
    // printf("shell~:");
    memset(buffer, 0, COMMAND_BUFFER_SIZE);
    commandStringSize = readCommand(buffer);
    if (commandStringSize == 0) {
      continue;
    }

    command = parseCommand(buffer, commandStringSize);

    if (command < 0) {
    } else {
      switch (command) {
        case 0:
          int pid = fork();
          if (pid == 0) {
            // child process
            while (1) {
            }
          } else {
            if (pid > 0) {
              // shell
              // pwait(pid);
            }
          }
          break;
        default:
          break;
      }
    }
  }
  return 0;
}
