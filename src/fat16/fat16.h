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

#ifndef _FAT16_H_
#define _FAT16_H_

#include <stddef.h>
#include <stdint.h>

#include "../drivers/disk.h"     // SECTOR_SIZE
#include "../process/process.h"  // struct process

#define FAT16_FILENAME_SIZE 8
#define FAT16_FILE_EXTENSION_SIZE 3
#define FAT16_ENTRY_EMPTY 0x0
#define FAT16_ENTRY_DELETED 0xE5
#define FAT16_FILE_EXTENSION_SIZE 3
#define FAT16_LAST_CLUSTER_VALUE 0xFFF7
#define FAT16_LONG_FILE_NAME_ATTRIBUTE 0x0F
#define MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES 512
#define MAX_SUPPORTED_FAT16_SECTORS_PER_CLUSTER 128
#define MAX_SUPPORTED_FAT16_TABLE_SECTORS 256
#define SECTOR_BUFFER_SIZE (256 * SECTOR_SIZE)

// Bios Parameter Block (BPB): FAT16 Header + FAT16 Extended Header

struct biosParameterBlock {
  uint8_t jumpAndNop[3];       // jmp short to bootloader code followed by nop
  uint8_t oemIdentifier[8];    // OS identifier
  uint16_t bytesPerSector;     // number of bytes per sector: 512
  uint8_t nSectorsPerCluster;  // number of sectors per cluster
  uint16_t nReservedSectors;   // number of reserved sectors: for this OS it
                               // includes boot sector, second sector (loader)
                              // and kernel image sectors
  uint8_t nFATs;             // number of FAT16 tables
  uint16_t nRootDirEntries;  // number of entries in root directory
  uint16_t nSectors;  // number of sectors for FAT16 partition if no larger than
                      // 65536
  uint8_t mediaType;  // media type code
  uint16_t nSectorsPerFAT;    // number of sectors occuipied by each FAT16 table
  uint16_t nSectorsPerTrack;  // number of sectors per magnetic Hard Disk track
  uint16_t nHeads;            // number of magnetic Hard Disk heads
  uint32_t nHiddenSsectors;   // number of hidden sectors
  uint32_t nSectorLarge;      // number of sectors for FAT16 partition if larger
                          // than 65536 Extended FAT16 Extended Header
  uint8_t
      driveNumber;    // drive number: 0x00 for floppy disk, 0x80 for hard disk
  uint8_t reserved;   // unused
  uint8_t signature;  // extended signature: must be 0x28 or 0x29 to indicate
                      // that the next 3 fields are valid
  uint32_t volumeId;  // partition serial number / identifier
  uint8_t volumeIdentifier[11];     // partition label
  uint8_t fileSystemIdentifier[8];  // file system label
} __attribute__((packed));

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

struct fileControlBlock {
  uint8_t name[FAT16_FILENAME_SIZE];       // name
  uint8_t ext[FAT16_FILE_EXTENSION_SIZE];  // extension
  uint16_t fat16ClusterIndex;              // FAT16 cluster index
  uint32_t fat16RootDirEntryIndex;         // FAT16 root directory
  uint32_t size;                           // file size
  uint32_t referenceCount;  // reference count: number of active file
                            // descriptors pointing to this File Control Block
};

struct fileDescriptor {
  struct fileControlBlock *fileControlBlockPtr;
  uint32_t seekPosition;  // last offset at which file was read or written to
  uint64_t nReferencingProcesses;  // counts number of processes using this file
                                   // descriptor
};

// Load file give input file name
// Returns 0 if successful, -1 otherwise
int64_t loadFile(char *name, uint8_t *fileBuffer);
// Open file given input file name
// Returns non negative descriptor or -1 if it failed
struct process;
int64_t openFile(struct process *proc, char *name);
// Open file given input file name
int64_t readFile(struct process *proc, uint64_t procFileDescriptorIndex,
                 uint8_t *fileBuffer, size_t size);
// Open file given input file name
int64_t closeFile(struct process *proc, uint32_t procFileDescriptorIndex);
// Returns file size
int64_t getFileSize(struct process *proc, uint32_t procFileDescriptorIndex);
// Loads and copies FAT16 root directory entries into input buffer and returns
// number of entries
int64_t getRootDirectory(struct fat16DirEntry *fat16DirEntryBuffer);
#endif
