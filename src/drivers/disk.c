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

#include "disk.h"

#include "../io/io.h"

// 24-bit ATA PIO

// Primary BUS ATA Programmed IO
#define ATA_PIO_DATA_REG 0x1F0
#define ATA_PIO_SECTOR_COUNT_REG 0x1F2
#define ATA_PIO_LBA_LOW_REG 0x1F3
#define ATA_PIO_LBA_MID_REG 0x1F4
#define ATA_PIO_LBA_HIGH_REG 0x1F5
#define ATA_PIO_DRIVE_REG 0x1F6
#define ATA_PIO_COMMAND_REG 0x1F7  // write
#define ATA_PIO_STATUS_REG 0x1F7   // read

#define ATA_PIO_MASTER_FLAG 0xE0
#define ATA_PIO_BSY_FLAG 0x08
#define ATA_PIO_READ_COMMAND 0x20

#include "../stdio/stdio.h"  // printk

// Assembly lock and unlock implementations for mutex
extern void spinLock(volatile uint8_t *lock);
extern void spinUnlock(volatile uint8_t *lock);

static volatile uint8_t diskLock =
    0;  // lock for SMP access to critical sections

// Read nSectors from disk starting from LBA address startSectorIndexLBA
int readSector(uint64_t startSectorIndexLBA, uint64_t nSectors, void *buffer) {
  spinLock(&diskLock);

  outb(ATA_PIO_DRIVE_REG,
       (startSectorIndexLBA >> 24) |
           ATA_PIO_MASTER_FLAG);  // send high bits 24 to 27  of LBA address
                                  // "ored" with master flag to disk controller
  outb(ATA_PIO_SECTOR_COUNT_REG,
       nSectors);  // send total number of sectors to be read to disk controller
  outb(ATA_PIO_LBA_LOW_REG,
       (uint8_t)(startSectorIndexLBA &
                 0xff));  // send bits 0 to 7 of LBA address to disk controller
  outb(ATA_PIO_LBA_MID_REG,
       (uint8_t)(startSectorIndexLBA >>
                 8));  // send bits 8 to 15 of LBA address to disk controller
  outb(ATA_PIO_LBA_HIGH_REG,
       (uint8_t)(startSectorIndexLBA >>
                 16));  // send bits 16 to 23 of LBA address to disk controller
  outb(ATA_PIO_COMMAND_REG,
       ATA_PIO_READ_COMMAND);  // send read command to disk controller

  uint16_t *bufPtr = (uint16_t *)buffer;

  for (int i = 0; i < nSectors; i++) {
    // Poll disk controller until ready
    uint8_t c = inb(ATA_PIO_STATUS_REG);
    while (!(c & ATA_PIO_BSY_FLAG)) {
      c = inb(ATA_PIO_STATUS_REG);
    }

    // Copy from hard disk to memory
    for (int j = 0; j < SECTOR_SIZE / sizeof(uint16_t); j++) {
      *bufPtr = inh(ATA_PIO_DATA_REG);
      bufPtr++;
    }
  }

  spinUnlock(&diskLock);

  return 0;
}
