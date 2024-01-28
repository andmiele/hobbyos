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

#ifndef _ACPI_H_
#define _ACPI_H_

#include <stdint.h>

// The Advanced Configuration and Power Interface (ACPI) is an open standard for
// operating systems for hardware component discovery, configuration, power
// management (e.g., sleep mode for hardware components), automatic
// configuration (e.g., Plug-and-Play and hot-swapping) and Gstatus monitoring

// IRQs
#define TIMER_IRQ 0x00
#define KEYBOARD_IRQ 0x01
#define MOUSE_IRQ 0x0C
#define SPURIOUS_IRQ 0x07

// Maximum number of CPU cores supported
#define MAX_N_CORES_SUPPORTED 16
// Maximum number of IO APICS supported
#define MAX_N_IO_APICS_SUPPORTED 1
// Maximum number of INTERRUPT OVERRIDES supported
#define MAX_N_INTERRUPT_OVERRIDE_SUPPORTED 16

/**** Local APIC defiintions ****/
// Register offsets
#define LAPIC_SPURIOUS_INT_VEC_REG 0xF0
#define LAPIC_DF_REG 0xE0      // Destination Format register
#define LAPIC_EOI_REG 0xB0     // End Of Interrupt (EO!) register
#define LAPIC_ICRHI_REG 0x310  // Interrupt command bits 32 : 64
#define LAPIC_ICRLO_REG 0x300  // Interrupt command bits 0 : 31
#define LAPIC_ID_REG 0x20      // Local APIC id register
#define LAPIC_LD_REG 0xD0      // Local Destination register
#define LAPIC_TP_REG 0x80      // Task Priority Register (TPR)

/* Interrupt command  */
// Bit position/shift of destination field
#define ICR_DESTINATION_BIT_POS 24
// Destination shortand options
#define ICR_NO_SHORTHAND 0x00000000
// Interrupt trigger mode (edge or level)
#define ICR_EDGE 0x00000000
// Level  (0-active: deassert or 1-active: assert)
#define ICR_ASSERT 0x00004000
// Destination mode (physical or logical)
#define ICR_PHYSICAL 0x00000000
// Delivery status
#define ICR_IDLE 0x00000000
#define ICR_SEND_PENDING 0x00001000
// Delivery mode
#define ICR_INIT 0x00000500
#define ICR_STARTUP 0x00000600
/**** END Local APIC definitions ****/

/**** IO APIC definitions ****/
// Memory-mapped register offsets
#define IOREGSEL 0x0  // register selection register
#define IOWIN 0x10    // data register
// Register codes
#define IOAPICID 0x0
#define IOAPICVER_AND_N_ENTRIES 0x01
#define IOAPICARB 0x2  // Arbitration id
// Two 32-bit registers per IRQ
#define IOAPICREDTBL(n) \
  (0x10 + 2 * n)  // lower 32 bits (add 1 for upper 32 bits)
/**** END IO APIC definitions ****/

// Initialize SMP cores
void smpInit();
// Search for and parse ACPI tables
int acpiInit();
// ACPI Power-off
int acpiShutdown(void);
// ACPI Timer based busy sleep (usecs)
void acpiBusySleepUsecs(uint64_t usecs);

// Initialize IO APIC
void ioAPICInit();
// Remap IRQ to input interrupt number; use low prio destination mode to have
// interrupt delivered to single CPU
void remapIRQ(uint32_t irq, uint8_t interrupt, uint8_t sendToSingleCPU);

// Initalize Local APIC
void localAPICInit();
// Get Local APIC identifier
uint32_t getLocalApicId();
// Send init command to local APIC indetified by localApicId
void localApicSendInitCommand(uint32_t localApicId);
// Send statup command to local APIC indetified by localApicId
void localApicSendStartupCommand(uint32_t localApicId, uint32_t vector);

// ACPI signature: this value denotes the start of the memory area containing
// the ACPI tables
#define ACPI_SIG 0x2052545020445352  // 'RSD PTR'

// FACP table signature
#define FACP_SIG 0x50434146  // 'PCAF'

// APIC table signature
#define APIC_SIG 0x43495041  // 'CIPA'

// DSDT signature
#define DSDT_SIG 0x54445344  // 'TDSD'

// S5 object signature
#define S5_SIG 0x5F35535F  // '_5S_'

// Start and end addresses of memory areas in which to search for the ACPI
// signature
#define BIOS_AREA_START_ADDR 0x000F0000
#define BIOS_AREA_END_ADDR 0x000FFFFF
#define EXTENDED_BIOS_AREA_START_ADDR 0x00080000
#define EXTENDED_BIOS_AREA_END_ADDR 0x0009FFFF

// Mask for enabling sleep mode (5)
#define ACPI_SLEEP_EN (1 << 13)

// ACPI TIMER frequency in Hertz
#define ACPI_TIMER_FREQ 3579545

// Root System Description Table (RSDT): contains pointers to the other
// description tables (DTs) The Header structure is shared by almost all ACPI
// tables
struct acpiRSDTHeader {
  uint8_t signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  uint8_t oemId[6];
  uint8_t oemTableId[8];
  uint32_t oemRevision;
  uint8_t creatorId[4];
  uint32_t creatorRevision;
} __attribute__((packed));

// Root System Description Pointer (RSDP) ACPI version 1.0 Header structure
struct acpiRSDP10Header {
  uint8_t signature[8];
  uint8_t checksum;
  uint8_t oemId[6];
  uint8_t revision;
  uint32_t rsdtAddress;
} __attribute__((packed));

// Root System Description Pointer (RSDP) ACPI version 2.0 Header structure
struct acpiRSDP20Header {
  struct acpiRSDP10Header rsdp10;
  uint32_t length;
  uint64_t xsdtAddress;
  uint8_t extendedChecksum;
  uint8_t reserved[3];
} __attribute__((packed));

// Generic Address Structure
// ACPI structure for the description of register position
struct genericAddressStructure {
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  uint8_t AccessSize;
  uint64_t Address;
} __attribute__((packed));

// (Fixed ACPI Description Table (FADT)
// This table is part of the ACPI programming interface
// It contains information about fixed register blocks used for power management

struct acpiFADT {
  struct acpiRSDTHeader header;
  uint32_t firmwareCtrl;
  uint32_t dsdt;

  // not used in ACPI > 1.0; for compatibility only
  uint8_t reserved;

  uint8_t preferredPowerManagementProfile;
  uint16_t sciInterrupt;
  uint32_t smiCommandPort;
  uint8_t acpiEnable;
  uint8_t acpiDisable;
  uint8_t s4BiosReq;
  uint8_t pStateControl;
  uint32_t pm1aEventBlock;
  uint32_t pm1bEventBlock;
  uint32_t pm1aControlBlock;
  uint32_t pm1bControlBlock;
  uint32_t pm2ControlBlock;
  uint32_t pmTimerBlock;
  uint32_t gpe0Block;
  uint32_t gpe1Block;
  uint8_t pm1EventLength;
  uint8_t pm1ControlLength;
  uint8_t pm2ControlLength;
  uint8_t pmtimerLength;
  uint8_t gpe0Length;
  uint8_t gpe1Length;
  uint8_t gpe1Base;
  uint8_t cStateControl;
  uint16_t worstC2Latency;
  uint16_t worstC3Latency;
  uint16_t flushSize;
  uint16_t flushStride;
  uint8_t dutyOffset;
  uint8_t dutyWidth;
  uint8_t dayAlarm;
  uint8_t monthAlarm;
  uint8_t century;

  // reserved in ACPI 1.0; used in ACPI >= 2.0
  uint16_t bootArchitectureFlags;

  uint8_t reserved2;
  uint32_t flags;

  // 12-byte struct
  struct genericAddressStructure resetReg;

  uint8_t resetValue;
  uint8_t reserved3[3];

  // 64-bit pointers - ACPI >= 2.0
  uint64_t xFirmwareControl;
  uint64_t xDsdt;

  struct genericAddressStructure xPM1aEventBlock;
  struct genericAddressStructure xPM1bEventBlock;
  struct genericAddressStructure xPM1aControlBlock;
  struct genericAddressStructure xPM1bControlBlock;
  struct genericAddressStructure xPM2ControlBlock;
  struct genericAddressStructure xPMTimerBlock;
  struct genericAddressStructure xgPE0Block;
  struct genericAddressStructure xgPE1Block;
} __attribute__((packed));

// ACPI Multiple APIC Description Table (MADT)
// This table describes the interrupt controllers in the system. It can be used
// to enumerate all the CPUs available on the system

struct acpiMADT {
  struct acpiRSDTHeader header;
  uint32_t localApicAddr;
  uint32_t flags;
} __attribute__((packed));

// **** APIC STRUCTURES **** //

// APIC struct type
#define APIC_TYPE_LOCAL_APIC 0
#define APIC_TYPE_IO_APIC 1
#define APIC_TYPE_INTERRUPT_OVERRIDE 2

struct apicHeader {
  uint8_t type;
  uint8_t length;
} __attribute__((packed));

struct localApic {
  struct apicHeader header;
  uint8_t acpiProcessorId;
  uint8_t apicId;
  uint32_t flags;
} __attribute__((packed));

struct ioApic {
  struct apicHeader header;
  uint8_t ioApicId;
  uint8_t reserved;
  uint32_t ioApicAddress;
  uint32_t globalSystemInterruptBase;
} __attribute__((packed));

struct apicInterruptOverride {
  struct apicHeader header;
  uint8_t bus;
  uint8_t source;
  uint32_t interrupt;
  uint16_t flags;
} __attribute__((packed));
#endif
