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

extern void exit();
extern int64_t fork();
extern int64_t exec(char *fileName);
extern void pwait(int64_t pid);

int main() {
  int64_t pid = fork();
  if (pid == 0) {
    printf("New process forked!\n");
    exec("TEST.BIN");
  } else {
    printf("Current process: wait for child process!\n");
    pwait(pid);
    printf("Child Process terminated!\n");
  }
  return 0;
}
