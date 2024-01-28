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

#ifndef _DISK_H_
#define _DISK_H_

#include <stdint.h>

#define SECTOR_SIZE 512

// Read nSectors from disk starting from LBA address startSectorIndexLBA
int readSector(uint64_t startSectorIndexLBA, uint64_t nSectors, void *buffer);

#endif