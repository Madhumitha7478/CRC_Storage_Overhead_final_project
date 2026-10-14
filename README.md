# CRC Storage Overhead for EEPROM

## 1. Project Overview

The CRC Storage Overhead project implements a mechanism to automatically append one CRC (Cyclic Redundancy Check) byte to each configured EEPROM data block before writing it to non-volatile memory.

The project calculates the total storage required for the original data and the additional CRC bytes, then checks whether the total requirement exceeds the EEPROM capacity of 512 bytes.

The project is developed using Embedded C on the NXP S32K144 microcontroller in S32 Design Studio. It uses the LPI2C driver to communicate with the M24C04 EEPROM and UART to display storage calculations and write status messages.

## 2. Objectives

- Configure EEPROM blocks with one CRC byte per block.
- Automatically calculate and append a CRC byte to each data block before writing.
- Calculate the total data storage requirement.
- Calculate the additional storage overhead introduced by CRC bytes.
- Verify that the total required storage does not exceed the 512-byte EEPROM capacity.
- Write the combined data and CRC buffer to EEPROM through I2C.
- Display storage calculations and write status through UART.

## 3. Architecture Diagram

```text
        Application Data
               |
               v
       EEPROM Block Config
               |
               v
       Calculate Data Size
               |
               v
       Calculate CRC Overhead
               |
               v
       Calculate Total Storage
               |
               v
      Check 512-Byte Capacity
          /             \
        Pass             Fail
         |                 |
         v                 v
   Calculate CRC      Reject Configuration
         |
         v
   Append CRC to Data
         |
         v
     LPI2C Write
         |
         v
      M24C04 EEPROM
         |
         v
   UART Status Output
```

**Architecture components:**

- **Application Data:** Provides the original data for each configured block.
- **Block Configuration:** Defines the block name, starting address, data size, and CRC size.
- **CRC Module:** Calculates an 8-bit CRC using the configured polynomial.
- **Storage Calculator:** Calculates data storage, CRC overhead, and total required storage.
- **Capacity Validation:** Checks whether the configuration fits within the available EEPROM capacity.
- **EEPROM Write Module:** Appends the CRC to the data buffer and writes the combined bytes through LPI2C.
- **UART Output:** Displays storage calculations and write status messages.

## 4. Hardware and Software Requirements

### Hardware Requirements

| Component | Description |
|---|---|
| Microcontroller | NXP S32K144 |
| Development Board | SmartWheels Micro EV2 |
| EEPROM | M24C04, 512-byte capacity |
| Communication Interface | I2C through LPI2C |
| Debug Interface | Onboard debugger |
| Computer | PC or laptop |

### Software Requirements

| Software | Purpose |
|---|---|
| S32 Design Studio | Embedded software development |
| Embedded C | CRC and EEPROM management implementation |
| S32K144 SDK | Peripheral driver support |
| LPI2C Driver | Communication with EEPROM |
| LPUART Driver | Displaying output through UART |
| PuTTY | Viewing UART output |
| Git and GitHub | Source-code management and project hosting |

## 5. Working Principle

### Step 1: Block Configuration

Each EEPROM block is configured using four fields:

- Block name
- Starting EEPROM address
- Original data size in bytes
- CRC size in bytes

The CRC size is configured as one byte for every block.

### Step 2: Storage Calculation

The program calculates the total size of all original data blocks and the additional CRC overhead.

The calculations are:

**Total Data Storage**

`dataTotal = sum of all block data sizes`

**CRC Storage Overhead**

`crcTotal = sum of all block CRC sizes`

**Total Storage Requirement**

`storageTotal = dataTotal + crcTotal`

The configuration must satisfy:

`storageTotal <= EEPROM_CAPACITY`

where `EEPROM_CAPACITY` is 512 bytes.

### Step 3: Capacity Validation

The program validates the block configuration, checks the address ranges, and checks for overlapping blocks. It also checks whether the calculated storage requirement fits within the EEPROM capacity.

If the configuration is invalid, the program rejects it before starting the EEPROM write loop.

### Step 4: CRC Calculation

The project uses CRC-8 with the following parameters:

| Parameter | Value |
|---|---|
| Polynomial | `0x07` |
| Initial CRC value | `0x00` |
| Final XOR value | `0x00` |
| CRC size | 1 byte |

The `CRC8_Calculate()` function processes the original data bytes and returns the CRC value.

### Step 5: Append CRC to Data

The `EEPROM_WriteBlock()` function copies the original data into a RAM buffer, calculates its CRC, and appends the CRC byte immediately after the original data.

For example, a block containing two data bytes requires three bytes of EEPROM storage: two data bytes and one CRC byte.

### Step 6: EEPROM Write

The `EEPROM_WriteBytes()` function transmits the combined data and CRC buffer through the LPI2C driver to the M24C04 EEPROM.

The write operation is divided at configured page boundaries and bank boundaries.

### Step 7: UART Status Output

The UART interface displays:

- Number of configured blocks
- Total original data storage
- Total CRC overhead
- Total storage requirement
- Capacity-check result
- EEPROM write status for each block

## 6. Example Configuration

The project uses three example EEPROM blocks.

| Block Name | Starting Address | Data Size | CRC Size | Total Storage |
|---|---:|---:|---:|---:|
| Vehicle Speed | `0x010` | 2 bytes | 1 byte | 3 bytes |
| Battery Level | `0x020` | 1 byte | 1 byte | 2 bytes |
| Temperature | `0x030` | 2 bytes | 1 byte | 3 bytes |
| **Total** | — | **5 bytes** | **3 bytes** | **8 bytes** |

### Storage Calculation

Total original data storage:

`2 + 1 + 2 = 5 bytes`

Total CRC overhead:

`1 + 1 + 1 = 3 bytes`

Total required storage:

`5 + 3 = 8 bytes`

EEPROM capacity:

`512 bytes`

Therefore, the example configuration requires 8 bytes out of the available 512 bytes and passes the total-storage capacity check.

## 7. Build and Run

1. Open the project in S32 Design Studio.
2. Verify the S32K144 target configuration and SDK dependencies.
3. Configure the EEPROM block names, addresses, data sizes, and CRC sizes.
4. Build the project and resolve any compilation errors.
5. Connect the development board and flash the generated firmware.
6. Open PuTTY and configure the appropriate UART COM port and baud rate.
7. Reset or run the target application.
8. Observe the data-storage calculation, CRC overhead, total storage, capacity result, and EEPROM write status.
9. Verify that the reported storage requirement is within the 512-byte limit.

## 8. Scope of the Project

- Automatic CRC-byte generation for configured EEPROM blocks.
- Calculation of additional memory required for CRC storage.
- Validation of EEPROM capacity before writing.
- Prevention of overlapping configured memory ranges.
- EEPROM write operations through the LPI2C interface.
- UART-based reporting of storage calculations and write status.

## 9. Expected Results

| Test Case | Expected Result |
|---|---|
| Valid three-block configuration | Configuration accepted |
| Original data totals 5 bytes | Data storage reports 5 bytes |
| Three blocks each reserve one CRC byte | CRC overhead reports 3 bytes |
| Total storage equals 8 bytes | Capacity check passes |
| Total storage exceeds 512 bytes | Configuration rejected before EEPROM writes |
| Invalid block address or overlapping blocks | Configuration rejected |
| EEPROM write driver reports success | Write success message displayed |
| EEPROM write driver reports an error | Write failure message displayed |

The expected UART output for the example configuration is:

```text
CRC STORAGE OVERHEAD PROJECT
-----------------------------
Configured blocks: 3
Data storage: 5 bytes
CRC overhead: 3 bytes
Total storage: 8 / 512 bytes
CAPACITY CHECK: PASS

Vehicle Speed: Data + CRC written successfully
Battery Level: Data + CRC written successfully
Temperature: Data + CRC written successfully
```

The exact output depends on the configuration and the result of each driver operation.

## 10. Limitations

- The current implementation writes the CRC but does not read the EEPROM data back to compare the stored CRC with a newly calculated CRC.
- CRC generation alone does not verify that the stored data remains uncorrupted.
- The CRC adds one byte of storage overhead per configured block.
- The EEPROM page size and I2C address configuration must match the actual hardware.
- The storage calculation covers configured data and CRC bytes, not additional memory reserved for unrelated application data.
- Successful driver status does not independently verify the correctness of the data physically stored in EEPROM.

## 11. Conclusion

The CRC Storage Overhead project demonstrates how to account for the additional EEPROM memory required to store CRC information alongside application data. By configuring one CRC byte per block, calculating the total data and CRC storage requirements, and checking the result against the 512-byte EEPROM capacity, the implementation provides a structured approach to EEPROM memory management.

The project also demonstrates CRC-8 calculation, automatic CRC appending, LPI2C-based EEPROM writes, and UART-based status reporting on the NXP S32K144 platform. It provides a foundation for further improvements, including EEPROM read-back verification and CRC-based data-integrity checking.
