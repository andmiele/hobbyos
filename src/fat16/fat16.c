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

#include "fat16.h"

#include "../drivers/disk.h"  //readSector
#include "../kernel.h"        // KERNEL_PANIC
#include "../lib/lib.h"       // memcpy
#include "../stdio/stdio.h"   // printk

// Minimal FAT16 implementation
// Subdirectories of the root directories are not supported at the moment
// Long File Names (LFN) not supported
// The root directory can only contain files whose name can be at most 8 bytes
// plus the dot followed by 3-byte extension (standard FAT16 file name size)
// Only file read operation is supported at the moment

// Assembly lock and unlock implementations for mutex
extern void spinLock(volatile uint8_t *lock);
extern void spinUnlock(volatile uint8_t *lock);

// returns core id
extern uint64_t getCoreId();  // ../kernel.asm

static uint8_t sectorBuffer[SECTOR_BUFFER_SIZE];
static struct biosParameterBlock bpb;
static struct fat16DirEntry
    rootDirEntries[MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES];
static uint16_t fat16Table[MAX_SUPPORTED_FAT16_TABLE_SECTORS *
                           SECTOR_SIZE];  // uint16_t is 2 bytes so this can
                                          // hold two FAT copies

static struct fileControlBlock
    fileControlBlockArray[MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES];
static struct fileDescriptor
    fileDescriptorArray[MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES];

volatile uint8_t
    fat16Lock;  // lock for SMP exclusive access to critical sections

// Get BIOS Parameter block (BPB) of primary master FAT16 disk
static struct biosParameterBlock *loadFAT16BPB() {
  struct biosParameterBlock *bpbPtr = NULL;
  readSector(0, 1, sectorBuffer);
  if (sectorBuffer[SECTOR_SIZE - 2] != 0x55 ||
      sectorBuffer[SECTOR_SIZE - 1] != 0xAA) {
    printk("ERROR CORE %d getFAT16BPB: invalid BIOS MBR signature\n",
           getCoreId());
    KERNEL_PANIC(ERR_PROCESS);
  }
  bpbPtr = &bpb;
  memcpy(bpbPtr, sectorBuffer, sizeof(struct biosParameterBlock));
  return bpbPtr;
}

// Get FAT16 root directory entry
static struct fat16DirEntry *loadFAT16rootDirPtr(
    struct biosParameterBlock *bpbPtr) {
  uint32_t rootDirSectorNumber =
      bpbPtr->nReservedSectors + bpbPtr->nFATs * bpbPtr->nSectorsPerFAT;
  uint32_t nEntries =
      (bpbPtr->nRootDirEntries > MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES
           ? MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES
           : bpbPtr->nRootDirEntries);
  uint32_t nSectorsToRead =
      ((nEntries * sizeof(struct fat16DirEntry)) / bpbPtr->bytesPerSector);
  if ((nEntries * sizeof(struct fat16DirEntry)) % bpbPtr->bytesPerSector) {
    nSectorsToRead++;
  }
  readSector(rootDirSectorNumber, nSectorsToRead, sectorBuffer);

  memcpy(rootDirEntries, sectorBuffer,
         sizeof(struct fat16DirEntry) * bpbPtr->nRootDirEntries);
  return rootDirEntries;
}

// Get FAT16 table
static uint16_t *loadFAT16Table(struct biosParameterBlock *bpbPtr) {
  uint16_t *fat16TablePtr = fat16Table;
  readSector(bpbPtr->nReservedSectors, bpbPtr->nSectorsPerFAT, fat16Table);
  return fat16TablePtr;
}

static int splitFilenameAndExtension(char *path, char *filename,
                                     char *extension) {
  int i;

  for (i = 0;
       (i < FAT16_FILENAME_SIZE) && (path[i] != '.') && (path[i] != '\0');
       i++) {
    if (path[i] == '/') {
      return -1;
    }

    filename[i] = path[i];
  }

  if (path[i] == '.') {  // parse extension
    i++;

    for (int j = 0; j < FAT16_FILE_EXTENSION_SIZE && path[i] != '\0';
         i++, j++) {
      if (path[i] == '/') {
        return -1;
      }

      extension[j] = path[i];
    }
  }

  if (path[i] != '\0') {
    return -1;
  }

  return 0;
}

// Returns index of  FAT16 directory entry for input file name
static int64_t findFileEntry(char *name, struct biosParameterBlock *bpbPtr,
                             struct fat16DirEntry *rootDirEntryPtr) {
  int64_t index = -1;
  char filename[FAT16_FILENAME_SIZE] = {"        "};
  char extension[FAT16_FILE_EXTENSION_SIZE] = {"   "};

  int64_t status = splitFilenameAndExtension(name, filename, extension);

  if (status == 0) {
    for (uint32_t i = 0; i < bpbPtr->nRootDirEntries; i++) {
      if ((rootDirEntryPtr[i].name[0] == FAT16_ENTRY_EMPTY) ||
          (rootDirEntryPtr[i].name[0] == FAT16_ENTRY_DELETED))
        continue;

      if (rootDirEntryPtr[i].attributes == FAT16_LONG_FILE_NAME_ATTRIBUTE) {
        continue;
      }
      if ((bufferEqual(rootDirEntryPtr[i].name, (uint8_t *)filename,
                       FAT16_FILENAME_SIZE)) &&
          (bufferEqual(rootDirEntryPtr[i].ext, (uint8_t *)extension,
                       FAT16_FILE_EXTENSION_SIZE))) {
        index = i;
        break;
      }
    }
  }

  return index;
}

// Read FAT16 clusters starting from clusterIndex until last cluster is reached
static int64_t readClusterData(uint16_t clusterIndex, size_t size,
                               uint32_t position, uint8_t *fileBuffer,
                               struct biosParameterBlock *bpbPtr,
                               uint16_t *fatTablePtr) {
  uint32_t nSectorsForRootDirEntries =
      ((bpbPtr->nRootDirEntries * sizeof(struct fat16DirEntry)) /
       bpbPtr->bytesPerSector);
  if ((bpbPtr->nRootDirEntries * sizeof(struct fat16DirEntry)) %
      bpbPtr->bytesPerSector) {
    nSectorsForRootDirEntries++;
  }
  uint32_t clusterSize = bpbPtr->nSectorsPerCluster * bpbPtr->bytesPerSector;
  size_t bytesRead = 0;

  uint32_t positionClusterCount = position / clusterSize;
  uint32_t positionClusterOffset = position % clusterSize;

  for (uint32_t i = 0; i < positionClusterCount; i++) {
    if (clusterIndex > MAX_SUPPORTED_FAT16_TABLE_SECTORS * SECTOR_SIZE) {
      printk("ERROR readClusterData: cluster index too large\n");
      return -1;
    }
    clusterIndex = fatTablePtr[clusterIndex];  // get index of next cluster
    if ((clusterIndex >= FAT16_LAST_CLUSTER_VALUE) ||
        (clusterIndex == 0)) {  // if this is not valid cluster
      printk("ERROR readClusterData: invalid first cluster\n");
      return -1;
    }
  }

  if (clusterIndex < 2) {
    printk(
        "ERROR readClusterData: FAT16 clusterIndex must be larger than or "
        "equal to 2\n");
    return -1;
  }

  // Read partial first cluster if offset is larger than zero
  if (positionClusterOffset != 0) {
    uint32_t partialClusterReadSize =
        (positionClusterOffset + size) < clusterSize
            ? size
            : (clusterSize - positionClusterOffset);
    uint32_t sectorIndex =
        bpbPtr->nReservedSectors + bpbPtr->nSectorsPerFAT * bpbPtr->nFATs +
        nSectorsForRootDirEntries +
        (clusterIndex - 2) *
            bpbPtr->nSectorsPerCluster;  // FAT16 cluster indices start at 2
    if (bpbPtr->nSectorsPerCluster * SECTOR_SIZE > SECTOR_BUFFER_SIZE) {
      printk(
          "ERROR readClusterData: number of sectors to read from disk too "
          "large for sector buffer\n");
      return -1;
    }
    if (partialClusterReadSize > size) {
      printk(
          "ERROR readClusterData: partialClusterReadSize larger than file "
          "size\n");
      return -1;
    }

    readSector(sectorIndex, bpbPtr->nSectorsPerCluster, sectorBuffer);
    memcpy(fileBuffer, (void *)(&(sectorBuffer[positionClusterOffset])),
           partialClusterReadSize);
    fileBuffer += partialClusterReadSize;
    if (clusterIndex > MAX_SUPPORTED_FAT16_TABLE_SECTORS * SECTOR_SIZE) {
      printk("ERROR readClusterData: cluster index too large\n");
      return -1;
    }

    clusterIndex = fatTablePtr[clusterIndex];  // get index of next cluster
    bytesRead = partialClusterReadSize;
  }

  while ((bytesRead < size) && (clusterIndex < FAT16_LAST_CLUSTER_VALUE)) {
    uint32_t sectorIndex =
        bpbPtr->nReservedSectors + bpbPtr->nSectorsPerFAT * bpbPtr->nFATs +
        nSectorsForRootDirEntries +
        (clusterIndex - 2) *
            bpbPtr->nSectorsPerCluster;  // FAT16 cluster indices start at 2
    if (bpbPtr->nSectorsPerCluster * SECTOR_SIZE > SECTOR_BUFFER_SIZE) {
      printk(
          "ERROR readClusterData: number of sectors to read from disk too "
          "large for sector buffer\n");
      return -1;
    }
    readSector(sectorIndex, bpbPtr->nSectorsPerCluster, sectorBuffer);

    if (bytesRead + size - bytesRead > size) {
      printk("ERROR readClusterData: total bytes read larger than file size\n");
      return -1;
    }
    if (clusterIndex > MAX_SUPPORTED_FAT16_TABLE_SECTORS * SECTOR_SIZE) {
      printk("ERROR readClusterData: cluster index too large\n");
      return -1;
    }
    clusterIndex = fatTablePtr[clusterIndex];  // get index of next cluster

    if (clusterIndex >=
        FAT16_LAST_CLUSTER_VALUE) {  // if we just read the last cluster, break

      if (bytesRead + size - bytesRead > size) {
        printk(
            "ERROR readClusterData: total bytes read larger than file size\n");
        return -1;
      }

      memcpy(fileBuffer, sectorBuffer, size - bytesRead);
      bytesRead += size - bytesRead;

      break;
    }
    if (bytesRead + clusterSize > size) {
      printk("ERROR readClusterData: total bytes read larger than file size\n");
      return -1;
    }

    memcpy(fileBuffer, sectorBuffer, clusterSize);
    fileBuffer += clusterSize;
    bytesRead += clusterSize;
  }

  return bytesRead;
}

// Load file given input file name
int64_t loadFile(char *name, uint8_t *fileBuffer) {
  int64_t status = -1;

  spinLock(&fat16Lock);
  struct biosParameterBlock *bpbPtr = loadFAT16BPB();
  uint16_t *fatTablePtr = loadFAT16Table(bpbPtr);
  struct fat16DirEntry *rootDirEntryPtr = loadFAT16rootDirPtr(bpbPtr);

  int64_t entryIndex = findFileEntry(name, bpbPtr, rootDirEntryPtr);

  if (entryIndex == -1) {
    printk("ERROR loadFile: file not found!\n");
    spinUnlock(&fat16Lock);
    return -1;
  }
  uint32_t readBytes = readClusterData(
      rootDirEntryPtr[entryIndex].startingClusterIndex,
      rootDirEntryPtr[entryIndex].fileSize, 0, fileBuffer, bpbPtr, fatTablePtr);
  if (readBytes == rootDirEntryPtr[entryIndex].fileSize) {
    status = 0;
  }
  spinUnlock(&fat16Lock);
  return status;
}

// Open file given input file name
int64_t openFile(struct process *proc, char *name) {
  int64_t procFileDescIndex = -1;
  int64_t fileDescIndex = -1;

  spinLock(&fat16Lock);
  struct biosParameterBlock *bpbPtr = loadFAT16BPB();
  struct fat16DirEntry *rootDirEntryPtr = loadFAT16rootDirPtr(bpbPtr);

  // find unused file descriptor in process array
  for (int i = 0; i < MAX_N_FILES_PER_PROCESS; i++) {
    if (proc->fileDescPtrArray[i] == NULL) {
      procFileDescIndex = i;
      break;
    }
  }

  if (procFileDescIndex == -1) {
    printk("ERROR openFile: no file descriptor for process available!\n");
    spinUnlock(&fat16Lock);
    return -1;
  }

  // find unused file descriptor in file descriptor table
  for (int i = 0; i < MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES; i++) {
    if (fileDescriptorArray[i].fileControlBlockPtr == NULL) {
      fileDescIndex = i;
      break;
    }
  }

  if (fileDescIndex == -1) {
    printk("ERROR openFile: no file descriptor available!\n");
    spinUnlock(&fat16Lock);
    return -1;
  }

  // search file in root directory

  int64_t entryIndex = findFileEntry(name, bpbPtr, rootDirEntryPtr);
  if (entryIndex == -1) {
    printk("ERROR openFile: file not found!\n");
    spinUnlock(&fat16Lock);
    return -1;
  }

  // if file is not open already, populate File Control Block
  if (fileControlBlockArray[entryIndex].referenceCount == 0) {
    fileControlBlockArray[entryIndex].fat16ClusterIndex =
        rootDirEntryPtr[entryIndex].startingClusterIndex;
    fileControlBlockArray[entryIndex].fat16RootDirEntryIndex = entryIndex;
    fileControlBlockArray[entryIndex].size =
        rootDirEntryPtr[entryIndex].fileSize;
    memcpy(fileControlBlockArray[entryIndex].name,
           rootDirEntryPtr[entryIndex].name, FAT16_FILENAME_SIZE);
    memcpy(fileControlBlockArray[entryIndex].ext,
           rootDirEntryPtr[entryIndex].ext, FAT16_FILE_EXTENSION_SIZE);
  }
  // increase reference count
  fileControlBlockArray[entryIndex].referenceCount++;

  memset(&fileDescriptorArray[fileDescIndex], 0, sizeof(struct fileDescriptor));
  fileDescriptorArray[fileDescIndex].fileControlBlockPtr =
      &fileControlBlockArray[entryIndex];
  fileDescriptorArray[fileDescIndex].nReferencingProcesses = 1;
  proc->fileDescPtrArray[procFileDescIndex] =
      &fileDescriptorArray[fileDescIndex];

  spinUnlock(&fat16Lock);
  return procFileDescIndex;
}

// Open file given input file name
int64_t readFile(struct process *proc, uint64_t procFileDescriptorIndex,
                 uint8_t *fileBuffer, size_t size) {
  spinLock(&fat16Lock);
  uint32_t position =
      proc->fileDescPtrArray[procFileDescriptorIndex]->seekPosition;
  uint32_t fileSize = proc->fileDescPtrArray[procFileDescriptorIndex]
                          ->fileControlBlockPtr->size;
  uint32_t bytesRead = 0;

  if (position + size > fileSize) {
    printk(
        "WARNING readFile: ((seek position) + (input size)) is larger than "
        "file size; only ((file size) - (position) + 1) bytes will be read\n");
    size = fileSize - position + 1;
  }
  struct biosParameterBlock *bpbPtr = loadFAT16BPB();
  uint16_t *fatTablePtr = fat16Table;
  bytesRead = readClusterData(proc->fileDescPtrArray[procFileDescriptorIndex]
                                  ->fileControlBlockPtr->fat16ClusterIndex,
                              size, position, fileBuffer, bpbPtr, fatTablePtr);
  proc->fileDescPtrArray[procFileDescriptorIndex]->seekPosition += bytesRead;

  spinUnlock(&fat16Lock);
  return bytesRead;
}

// Open file given input file name
int64_t closeFile(struct process *proc, uint32_t procFileDescriptorIndex) {
  if (procFileDescriptorIndex >= MAX_N_FILES_PER_PROCESS) {
    printk(
        "ERROR closeFile: process file descriptor index must be smaller "
        "than MAX_N_FILES_PER_PROCESS: d!\n",
        MAX_N_FILES_PER_PROCESS);
    return -1;
  }
  if (proc->fileDescPtrArray[procFileDescriptorIndex] == NULL) {
    printk("ERROR closeFile: null file descriptor pointer!\n");
    return -1;
  }

  spinLock(&fat16Lock);
  if (proc->fileDescPtrArray[procFileDescriptorIndex]
          ->fileControlBlockPtr->referenceCount <= 0) {
    printk(
        "ERROR closeFile: file reference count less than or equal to zero!\n");
    spinUnlock(&fat16Lock);
    return -1;
  }

  proc->fileDescPtrArray[procFileDescriptorIndex]
      ->fileControlBlockPtr->referenceCount--;
  proc->fileDescPtrArray[procFileDescriptorIndex]->nReferencingProcesses--;
  if (proc->fileDescPtrArray[procFileDescriptorIndex]->nReferencingProcesses ==
      0) {  // if the number of processes using this file descriptor is zero,
            // set File Control Block ptr to null to free it up
    proc->fileDescPtrArray[procFileDescriptorIndex]->fileControlBlockPtr = NULL;
  }
  proc->fileDescPtrArray[procFileDescriptorIndex] = NULL;
  spinUnlock(&fat16Lock);
  return 0;
}

// Returns file size
int64_t getFileSize(struct process *proc, uint32_t procFileDescriptorIndex) {
  if (procFileDescriptorIndex >= MAX_N_FILES_PER_PROCESS) {
    printk(
        "ERROR getFileSize: process file descriptor index must be smaller "
        "than MAX_N_FILES_PER_PROCESS: d!\n",
        MAX_N_FILES_PER_PROCESS);
    return -1;
  }
  if (proc->fileDescPtrArray[procFileDescriptorIndex] == NULL) {
    printk("ERROR getFileSize: null file descriptor pointer!\n");
    return -1;
  }

  spinLock(&fat16Lock);
  int64_t size = proc->fileDescPtrArray[procFileDescriptorIndex]
                     ->fileControlBlockPtr->size;

  spinUnlock(&fat16Lock);
  return size;
}
// Loads and copies FAT16 root directory entries into input buffer and returns
// number of entries
int64_t getRootDirectory(struct fat16DirEntry *fat16DirEntryBuffer) {
  spinLock(&fat16Lock);
  struct biosParameterBlock *bpbPtr = loadFAT16BPB();
  struct fat16DirEntry *rootDirEntryPtr = loadFAT16rootDirPtr(bpbPtr);

  memcpy(fat16DirEntryBuffer, rootDirEntryPtr,
         (bpbPtr->nRootDirEntries > MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES
              ? MAX_SUPPORTED_FAT16_ROOT_DIR_ENTRIES
              : bpbPtr->nRootDirEntries) *
             sizeof(struct fat16DirEntry));

  spinUnlock(&fat16Lock);
  return bpbPtr->nRootDirEntries;
}
