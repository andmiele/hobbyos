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

#include "acpi.h"

#include "io/io.h"
#include "memory/memory.h"
#include "stdio/stdio.h"

extern uint8_t *gLocalApicAddress;
extern uint64_t gActiveCpuCount;

// global variables used by kernel
uint32_t acpiPM1aControlBlock;
uint32_t acpiPM1bControlBlock;
uint16_t acpiSleepTypeA;
uint16_t acpiSleepTypeB;
uint32_t acpiSmiCommandPort;
uint64_t acpiPMTimerPort;
uint8_t acpiPMTimerExtended;  // Flag for extended ACPI timer (24-bit / 32-bit)
uint32_t acpiNCores;
uint32_t acpiCoreIds[MAX_N_CORES_SUPPORTED];
uint32_t acpiNIoApics;
uint8_t *ioApicAddresses[MAX_N_IO_APICS_SUPPORTED];  // FIXME: might want to
                                                     // move this
uint32_t apicNInterruptOverrides;
struct apicInterruptOverride
    *apicInterruptOverridePtrs[MAX_N_INTERRUPT_OVERRIDE_SUPPORTED];

// Initalize SMP:
// send init and startup commands to all AP cores
// wait until all AP cores have started
void smpInit() {
  gActiveCpuCount = 1;
  uint32_t localCoreId = getLocalApicId();

  // send init command to all APs
  for (int i = 0; i < acpiNCores; ++i) {
    uint32_t currId = acpiCoreIds[i];
    if (currId != localCoreId) {
      localApicSendInitCommand(currId);
    }
  }

  // wait 10ms
  acpiBusySleepUsecs(10000);

  // send startup command to all APs
  for (int i = 0; i < acpiNCores; ++i) {
    uint32_t currId = acpiCoreIds[i];
    if (currId != localCoreId) {
      localApicSendStartupCommand(currId, 0x8);
    }
  }

  // wait 1ms
  acpiBusySleepUsecs(1000);

  printk("Wait for AP cores!\n");
  while (gActiveCpuCount < acpiNCores) {
    // wait 1ms
    acpiBusySleepUsecs(1000);
  }
  printk("AP cores activated!\n");
}

// Get the current ACPI timer
volatile uint32_t acpiGetTimerValue(void) { return inw(acpiPMTimerPort); }

// Get the current ACPI timer period
uint64_t acpiGetTimerPeriod(void) {
  if (acpiPMTimerExtended) {
    return (((uint64_t)1) << 32);
  } else {
    return (1 << 24);
  }
}

// ACPI Timer based sleep function (usecs)
void acpiBusySleepUsecs(uint64_t usecs) {
  uint64_t ticks;
  volatile uint64_t count;
  volatile uint64_t curr;
  volatile uint64_t prev;

  // clock ticks to be counted
  ticks = (ACPI_TIMER_FREQ * usecs) / 1000000;

  prev = acpiGetTimerValue();
  count = 0;
  while (count < ticks) {
    curr = acpiGetTimerValue();
    if (curr < prev) {  // handle timer wrap-around
      count += acpiGetTimerPeriod() + curr - prev;
    } else {
      count += curr - prev;
    }
    prev = curr;
    // x64 pause instruction informs the CPU that this is a spin-lock wait loop
    // so memory and cache accesses could be optimized (e.g., reduce
    // mispredicted out-of-order memory accesses or save power)
    __asm volatile("pause" ::: "memory");
  }
}

// Compute ACPI checksum
uint8_t checksum(const uint8_t *ptr, size_t length) {
  uint8_t c = 0;
  for (int i = 0; i < length; i++) c += ptr[i];
  return c;
}

// Enable ACPI: based on ACPI spec

int enableACPI(struct acpiFADT *fadtPtr) {
  // If bit zero in PM1A control block is set, ACPI is already enabled
  if ((inw(fadtPtr->pm1aControlBlock) & 1) == 0) {
    printk("ACPI already enabled!\n");
    return 0;
  }
  // If System Management Interrupt (SMI) command register is not set, ACPI mode
  // transition is not supported
  if (fadtPtr->smiCommandPort == 0) {
    printk(
        "ERROR: ACPI mode transition not supported (SMI command register is "
        "zero)\n");
    return -1;
  }
  // If either of the acpiEnable or acpiDisable registers is not set then ACPI
  // enable/disable transitions are not supported

  if (fadtPtr->acpiEnable == 0 || fadtPtr->acpiDisable == 0) {
    printk("ERROR: ACPI enable or disable tranistion not supported\n");
    return -1;
  }

  outb(fadtPtr->smiCommandPort, 1);  // enable ACPI command
  // On some systems it can take some time for ACPI to be enabled
  // So give it some time

  int i = 0;
  int iCount = 300;

  // check PM1 A control block for some time seconds
  for (i = 0; i < iCount; i++) {
    if (inw(fadtPtr->pm1aControlBlock) & 1) break;
    acpiBusySleepUsecs(10000);
  }
  if (i == iCount) {
    printk("ERROR: ACPI could not be activated (PM1a ctrl bit not set)");
    return -1;
  }

  // check PM1 B control block for about 3 seconds
  if (fadtPtr->pm1bControlBlock) {
    for (i = 0; i < iCount; i++) {
      if (inw(fadtPtr->pm1bControlBlock) & 1) break;
      acpiBusySleepUsecs(10000);
    }
    if (i == iCount) {
      printk("ERROR: ACPI could not be activated (PM1b ctrl bit not set)");
      return -1;
    }
  }

  printk("ACPI enabled!\n");
  return 0;
}

int parseDSDT(const struct acpiRSDTHeader *rsdtPtr, size_t fadtPtrSize) {
  int32_t length = rsdtPtr->length - sizeof(struct acpiRSDTHeader);

  uint32_t signature = *((uint32_t *)rsdtPtr->signature);

  if (signature == DSDT_SIG) {
    printk("Found ACPI DSDT table!\n");
  }

  uint8_t *ptr8 = (uint8_t *)(uintptr_t)rsdtPtr;

  uint8_t c = checksum(ptr8, rsdtPtr->length);
  if (c) {
    printk("ERROR: ACPI DSDT Checksum not zero\n");
    return -1;
  }

  // Skip header
  ptr8 += sizeof(struct acpiRSDTHeader);

  // Search for S5 object signature
  while (length > 0) {
    uint32_t s5Sig = *((uint32_t *)ptr8);
    if (s5Sig == S5_SIG) {
      break;
    }
    ++ptr8;
    --length;
  }
  if (length > 0) {
    printk("ACPI S5 object signature found!\n");

    // check validity of ACPI Machine Language (AML) object structure

    // From osdev.org and ACPI spec
    // //
    // // Structure of  \_S5 object
    // // -----------------------------------------
    // //        | (optional) |    |    |    |
    // // NameOP | \          | _  | S  | 5  | _
    // // 0x08   | 0x5A       |0x5F|0x53|0x35|0x5F
    // //
    // //
    // -----------------------------------------------------------------------------------------------------------
    // //           |           |              | ( SLP_TYPa)         | (
    // SLP_TYPb)         | (Reserved)      | (Reserved)
    // // PackageOP | PkgLength | NumElements  | byte prefix and Num | byte
    // prefix and num | byte prefix num | byte prefix and num
    // // 0x12      | 0x0A      | 0x04         | 0x0A              05| 0x0A 05|
    // 0x0A          05| 0x0A             05
    // //
    // // this structure was also encountered
    // // PackageOP | PkgLength | NumElements |
    // // 0x12      | 0x06      | 0x04        | 00 00 00 00
    // //

    // PkgLength:
    // bits 7 - 6: number of bytes that follows this field (0-3)
    // bits 5 - 4: only used if pkglength < 63
    // bits 3 - 0: least significant package length nibble
    // Note
    // The high 2 bits of the first byte reveal how many following bytes are
    // used for PkgLength. If PkgLength is a single byte, bits 0 - 5 are used to
    // encode the package length (0 - 63). If the package length value is more
    // than 63, more than one byte must be used for the encoding in which case
    // bits 4 and 5 of the firs byte are reserved and must be zero. If the
    // multiple bytes are used, bits 0 - 3 of the first byte become the least
    // significant 4 bits of the resulting package length value. The next byte
    // represents the least significant 8 bits of the resulting value and so on,
    // up to 3 bytes. Thus, the maximum package length is 2**28.

    // Check for presence of NameOP and optional \ and PackageOP
    if ((*(ptr8 - 1) == 0x08 || (*(ptr8 - 2) == 0x08 && *(ptr8 - 1) == '\\')) &&
        *(ptr8 + 4) == 0x12) {
      ptr8 += 5;  // move to PkgLength
      ptr8 += (((*ptr8) & 0xC0) >> 6) +
              2;  // calculate PkgLength size, take bits 7 - 6, assume there are
      // at most 4 following bytes (+ 2 is for PackageOP and
      // PkgLength fields)

      if ((*ptr8) == 0x0A) ptr8++;  // skip byte prefix
      acpiSleepTypeA = (*ptr8) << 10;
      ++ptr8;

      if ((*ptr8) == 0x0A) ptr8++;  // skip byte prefix
      acpiSleepTypeB = (*ptr8) << 10;

      return 0;

    } else {
      printk("ERROR: valid S5 object struct not found\n");
      return -1;
    }
  } else {
    printk("ERROR: ACPI S5 object not found\n");
    return -1;
  }
}

// Root System Description Pointer (RSDP) found: parse ACPI tables
int parseAcpiTables(const uint8_t *ptr) {
  printk("ACPI RSDP found! Verifying checksum...\n");

  uint8_t c = checksum(ptr, sizeof(struct acpiRSDP10Header));
  // as per the UEFI ACPI spec only header is included in checksum
  if (c) {
    printk("ERROR: ACPI RSDP Checksum not zero\n");
    return -1;
  }

  printk("ACPI RSDP checksum == 0! Read OEMID...\n");

  struct acpiRSDP10Header *rsdp10Ptr = (struct acpiRSDP10Header *)ptr;

  // Print OEMID
  char oemIdString[8];
  oemIdString[6] = '\n';  // newline
  oemIdString[7] = '\0';  // string null terminator
  for (int i = 0; i < 6; i++) oemIdString[i] = rsdp10Ptr->oemId[i];

  printk("OEM ID = ");
  printk(oemIdString);

  if (rsdp10Ptr->revision == 0)  // ACPI version 1.0
  {
    printk("ACPI version 1.0\n");
  } else if (rsdp10Ptr->revision == 2)  // ACPI version 2.0
  {
    printk("ACPI version 2.0 detected!\n");
  } else {
    printk("ERROR: Unknown ACPI version\n");
    return -1;
  }

  struct acpiRSDP20Header *rsdp20Ptr = (struct acpiRSDP20Header *)rsdp10Ptr;

  size_t fadtPtrSize = 4;
  struct acpiRSDTHeader *rsdtPtr = NULL;
  if ((rsdp10Ptr->revision == 0) ||
      (rsdp10Ptr->revision == 2 && rsdp20Ptr->xsdtAddress == 0)) {
    // Parse RSDT
    rsdtPtr = (struct acpiRSDTHeader *)(uintptr_t)rsdp10Ptr->rsdtAddress;
    fadtPtrSize = 4;  // 32-bit pointer
  } else {
    // ACPI version 2.0, parse XSDT
    rsdtPtr = (struct acpiRSDTHeader *)(uintptr_t)rsdp20Ptr->xsdtAddress;
    fadtPtrSize = 8;  // 64-bit pointer
  }
  // Print RSDT OEM ID
  for (int i = 0; i < 6; i++) oemIdString[i] = rsdtPtr->oemId[i];

  printk("RSDT OEM ID = ");
  printk(oemIdString);

  // print rsdt oem table id
  char oemTableIdString[10];
  oemTableIdString[8] = '\n';  // newline
  oemTableIdString[9] = '\0';  // string null terminator
  for (int i = 0; i < 8; i++) oemTableIdString[i] = rsdtPtr->oemTableId[i];

  printk("RSDT OEM TABLE ID = ");
  printk(oemTableIdString);

  // parse description tables (DT)
  // look for 32-bit DT signature

  // skip current RSDT (header)
  uint8_t *ptr8 = ((uint8_t *)(rsdtPtr)) + (sizeof(struct acpiRSDTHeader));

  uint8_t *ptr8End = ((uint8_t *)(rsdtPtr)) + (rsdtPtr->length);

  while (ptr8 != ptr8End) {
    // remember that RSDT contains 32-bit addresses of other DTs after the
    // header, so cast 32-bit value/address pointed to by ptr32 to struct
    // RSDTHeader pointer
    struct acpiRSDTHeader *currRsdtPtr = NULL;
    if (fadtPtrSize == 4)  // ACPI version 1.0
      currRsdtPtr = (struct acpiRSDTHeader *)(uintptr_t)(*((uint32_t *)ptr8));
    else
      currRsdtPtr = (struct acpiRSDTHeader *)(uintptr_t)(*((uint64_t *)ptr8));

    uint32_t signature = *((uint32_t *)currRsdtPtr->signature);

    if (signature == FACP_SIG) {
      printk("Found ACPI FACP table!\n");
      struct acpiFADT *fadtPtr = (struct acpiFADT *)currRsdtPtr;
      // verify checksum
      uint8_t c = checksum((uint8_t *)fadtPtr, currRsdtPtr->length);
      if (c) {
        printk("ERROR: ACPI FACP checksum not zero\n");
        return -1;
      }

      enableACPI(fadtPtr);

      acpiPM1aControlBlock = fadtPtr->pm1aControlBlock;
      acpiPM1bControlBlock = fadtPtr->pm1bControlBlock;
      acpiSmiCommandPort = fadtPtr->smiCommandPort;
      acpiPMTimerPort = fadtPtr->pmTimerBlock;
      acpiPMTimerExtended =
          (fadtPtr->flags >> 8) &
          1;  // Flag for extended ACPI timer (24-bit / 32-bit)
      // parse DSDT
      // search for S5 object and parse shutdown info
      struct acpiRSDTHeader *nextRSDTHeader =
          ((struct acpiRSDTHeader *)(uintptr_t)fadtPtr->dsdt);
      if (parseDSDT(nextRSDTHeader, fadtPtrSize)) {
        return -1;
      }
    } else if (signature == APIC_SIG) {
      printk("Found ACPI APIC table (MADT)!\n");

      struct acpiMADT *madtPtr = (struct acpiMADT *)currRsdtPtr;

      if (madtPtr->flags & 0x1) {
        printk("MADT flags: 8259 Legacy PIC mode enabled. Disabling PIC!\n");
        outb(0x22, 0x70);  // Select interrupt mode control register(IMCR)
        outb(0x23, 0x01);  // Force all interrupts (maskable and non-maskable)
                           // to be handled by APIC
      }
      // verify checksum
      uint8_t c = checksum((uint8_t *)madtPtr, currRsdtPtr->length);
      if (c) {
        printk("ERROR: ACPI APIC MADT checksum not zero\n");
        return -1;
      }

      gLocalApicAddress = (uint8_t *)(uintptr_t)madtPtr->localApicAddr;
      uint8_t *apicPtr8 = ((uint8_t *)madtPtr) + sizeof(struct acpiMADT);
      uint8_t *apicPtr8End = ((uint8_t *)madtPtr) + madtPtr->header.length;

      acpiNCores = 0;
      acpiNIoApics = 0;
      apicNInterruptOverrides = 0;
      while (apicPtr8 < apicPtr8End) {
        struct apicHeader *header = (struct apicHeader *)apicPtr8;
        uint8_t type = header->type;
        uint8_t length = header->length;

        if (type == APIC_TYPE_LOCAL_APIC) {
          if (acpiNCores < MAX_N_CORES_SUPPORTED) {
            struct localApic *localApicPtr = (struct localApic *)apicPtr8;
            if (localApicPtr->flags & 0x1) {
              printk("Found CPU local APIC!\n");
              acpiCoreIds[acpiNCores] = localApicPtr->apicId;
              ++acpiNCores;
            } else {
              printk("WARNING: Found disabled CPU local APIC (ignored)\n");
            }
          } else
            printk(
                "WARNING: Found CPU local APIC but exceeded number of cores "
                "supported\n");
        } else if (type == APIC_TYPE_IO_APIC) {
          if (acpiNIoApics < MAX_N_IO_APICS_SUPPORTED) {
            printk("Found IO APIC!\n");
            struct ioApic *ioApicPtr = (struct ioApic *)apicPtr8;
            ioApicAddresses[acpiNIoApics] =
                (uint8_t *)(uintptr_t)ioApicPtr->ioApicAddress;
            ++acpiNIoApics;
          } else {
            printk(
                "WARNING: Found IO APIC but exceeded number of IO APICS "
                "supported\n");
          }
        } else if (type == APIC_TYPE_INTERRUPT_OVERRIDE) {
          if (apicNInterruptOverrides < MAX_N_INTERRUPT_OVERRIDE_SUPPORTED) {
            //            printk("Found APIC interrupt override!\n");
            apicInterruptOverridePtrs[apicNInterruptOverrides] =
                (struct apicInterruptOverride *)apicPtr8;
            apicNInterruptOverrides++;
          } else {
            printk(
                "WARNING: Found APIC interrupt override but exceeded number "
                "of interrupt overrides "
                "supported\n");
          }
        } else {
          printk("WARNING: Found unsupported APIC struct (ignored)\n");
        }
        apicPtr8 += length;
      }
      if (acpiNCores == 0) {
        printk("ERROR: no ACPI Local APICS found\n");
        return -1;
      }
      if (acpiNIoApics == 0) {
        printk("ERROR: no ACPI IO APICS found\n");
        return -1;
      }

      printk("Finished parsing APIC MADT!\n");
    }
    ptr8 += fadtPtrSize;
  }

  printk("Finished search for ACPI FADT and APIC tables!\n");
  return 0;
}

// Search for ACPI signature and parse Root System Description Pointer (RSDP)
int acpiInit() {
  uint64_t *ptr = (uint64_t *)BIOS_AREA_START_ADDR;
  uint64_t *end = (uint64_t *)BIOS_AREA_END_ADDR;

  // search main BIOS area first

  while (ptr < end) {
    uint64_t sig = *ptr;
    if (sig == ACPI_SIG) {
      if (parseAcpiTables((uint8_t *)ptr) == 0) {
        return 0;
      } else
        return -1;
    }

    ptr++;
  }

  // search extended BIOS area
  ptr = (uint64_t *)EXTENDED_BIOS_AREA_START_ADDR;
  end = (uint64_t *)EXTENDED_BIOS_AREA_END_ADDR;

  while (ptr < end) {
    uint64_t sig = *ptr;
    if (sig == ACPI_SIG) {
      if (parseAcpiTables((uint8_t *)ptr) == 0) {
        return 0;
      } else
        return -1;
    }

    ptr++;
  }

  printk("ERROR: ACPI signature not found\n");
  return -1;
}

// ACPI Power-off
int acpiShutdown(void) {
  // Try 30 times with a 100 ms interval
  int iCount = 30;

  if (acpiPM1aControlBlock) {
    for (int i = 0; i < iCount; i++) {
      outh(acpiPM1aControlBlock, acpiSleepTypeA | ACPI_SLEEP_EN);
      // Sleep for 1 sec
      acpiBusySleepUsecs(100000);
    }
  }

  if (acpiPM1bControlBlock) {
    for (int i = 0; i < iCount; i++) {
      outh(acpiPM1bControlBlock, acpiSleepTypeB | ACPI_SLEEP_EN);
      // Sleep for 1 sec
      acpiBusySleepUsecs(100000);
    }
  }
  return -1;
}

/**** IO APIC ****/

static void ioAPICSetEntry(uint8_t *ioAPICAddress, uint8_t entryIndex,
                           uint64_t value) {
  // Select register
  *((volatile uint32_t *)(ioAPICAddress + IOREGSEL)) = IOAPICREDTBL(entryIndex);
  // Write low 32 bits
  *((volatile uint32_t *)(ioAPICAddress + IOWIN)) = (uint32_t)value;

  // Select register
  *((volatile uint32_t *)(ioAPICAddress + IOREGSEL)) =
      IOAPICREDTBL(entryIndex) + 1;
  // Write high 32 bits
  *((volatile uint32_t *)(ioAPICAddress + IOWIN)) = (uint32_t)(value >> 32);
}

void ioAPICInit() {
  // Determine number of entries supported by IO APIC
  // Select register
  *((volatile uint32_t *)(ioApicAddresses[0] + IOREGSEL)) =
      IOAPICVER_AND_N_ENTRIES;
  uint32_t versionAndNumEntries =
      *((volatile uint32_t *)(ioApicAddresses[0] + IOWIN));
  uint32_t numEntries = ((versionAndNumEntries >> 16) & 0xFF) + 1;

  // Mask all IRQs: set bit 16
  for (int i = 0; i < numEntries; i++) {
    ioAPICSetEntry(ioApicAddresses[0], i, 1 << 16);
  }
  printk("IOAPIC Init: Masked all IRQs!\n");
}
/**** END IO APIC ****/
/**** IRQ OVERRIDE ****/

// Remap IRQ to input interrupt number; use low prio destination mode to have
// interrupt delivered to single CPU
void remapIRQ(uint32_t irq, uint8_t interrupt, uint8_t sendToSingleCPU) {
  uint32_t remappedIRQ = irq;
  for (int i = 0; i < apicNInterruptOverrides; i++)
    if (apicInterruptOverridePtrs[i]->source == irq) {
      remappedIRQ = apicInterruptOverridePtrs[i]->interrupt;
      printk("Found Interrupt Override! Remapping IRQ\n");
      break;
    }
  // All flags set to zero. 11: 1 Logical mode
  // Destination 63-56: 0xFF Logical broadcast (at most 16 CPUs will receive the
  // interrupt (max number))
  uint64_t flags = 0xFF00000000000800;  // delivery mode is 000: fixed
                                        // (broadcast)
  if (sendToSingleCPU) {
    flags = 0xFF00000000000900;  // delivery mode is 001: low prio (CPU with
                                 // lowest prio)
  }
  ioAPICSetEntry(ioApicAddresses[0], remappedIRQ, flags | interrupt);
}

/**** END IRQ OVERRIDE ****/
/**** LOCAL APIC ****/

uint32_t getLocalApicId() {
  return *((volatile uint32_t *)(gLocalApicAddress + LAPIC_ID_REG)) >> 24;
}

void localApicSendInitCommand(uint32_t localApicId) {
  *(volatile uint32_t *)(gLocalApicAddress + LAPIC_ICRHI_REG) =
      localApicId << ICR_DESTINATION_BIT_POS;
  *(volatile uint32_t *)(gLocalApicAddress + LAPIC_ICRLO_REG) =
      ICR_INIT | ICR_PHYSICAL | ICR_ASSERT | ICR_EDGE | ICR_NO_SHORTHAND;
  while ((*(volatile uint32_t *)(gLocalApicAddress + LAPIC_ICRLO_REG)) &
         ICR_SEND_PENDING)
    ;
}
void localApicSendStartupCommand(uint32_t localApicId, uint32_t vector) {
  *(volatile uint32_t *)(gLocalApicAddress + LAPIC_ICRHI_REG) =
      localApicId << ICR_DESTINATION_BIT_POS;
  *(volatile uint32_t *)(gLocalApicAddress + LAPIC_ICRLO_REG) =
      vector | ICR_STARTUP | ICR_PHYSICAL | ICR_ASSERT | ICR_EDGE |
      ICR_NO_SHORTHAND;
  while ((*(volatile uint32_t *)(gLocalApicAddress + LAPIC_ICRLO_REG)) &
         ICR_SEND_PENDING)
    ;
}
void localAPICInit() {
  // Clear task priority register
  *((volatile uint32_t *)(gLocalApicAddress + LAPIC_TP_REG)) = 0x0;
  *((volatile uint32_t *)(gLocalApicAddress + LAPIC_DF_REG)) =
      0xFFFFFFFF;  // Flat mode
  *((volatile uint32_t *)(gLocalApicAddress + LAPIC_LD_REG)) =
      0x01000000;  // Use id 1 for all CPUs
  // Set Spurious Interrupt Vector Register bit 8 to start receiving interrupts
  uint32_t currVal =
      *((volatile uint32_t *)(gLocalApicAddress + LAPIC_SPURIOUS_INT_VEC_REG));
  currVal |= 0x1FF;
  *((volatile uint32_t *)(gLocalApicAddress + LAPIC_SPURIOUS_INT_VEC_REG)) =
      currVal;
}
/**** END LOCAL APIC ***/
