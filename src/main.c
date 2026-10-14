
#include "sdk_project_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* EEPROM configuration */
#define EEPROM_CAPACITY       512U
#define EEPROM_BASE_ADDRESS   0x50U
#define EEPROM_BANK_SIZE      256U
#define EEPROM_PAGE_SIZE      8U
#define EEPROM_I2C            0U
#define EEPROM_TIMEOUT        100U

/* One CRC byte per block */
#define CRC_SIZE              1U

/* Block configuration */
typedef struct
{
    const char *name;
    uint16_t address;
    uint16_t data_size;
    uint8_t crc_size;
} EepromBlock;

/* Example blocks: replace with your required configuration */
static const EepromBlock blocks[] =
{
    {"Vehicle Speed", 0x010U, 2U, CRC_SIZE},
    {"Battery Level", 0x020U, 1U, CRC_SIZE},
    {"Temperature",   0x030U, 2U, CRC_SIZE}
};

#define BLOCK_COUNT (sizeof(blocks) / sizeof(blocks[0]))

/* Sample data corresponding to the configured blocks */
static const uint8_t speedData[]       = {0x12U, 0x34U};
static const uint8_t batteryData[]     = {0x64U};
static const uint8_t temperatureData[] = {0x19U, 0x2AU};

static const uint8_t *const blockData[] =
{
    speedData,
    batteryData,
    temperatureData
};

static lpi2c_master_state_t lpi2cMasterState;
static lpuart_state_t uartState;


/* Print messages over UART */
static void UART_Print(const char *message)
{
    uint32_t length = 0U;

    while (message[length] != '\0')
    {
        length++;
    }

    if (length > 0U)
    {
        (void)LPUART_DRV_SendDataBlocking(
            INST_LPUART_1,
            (const uint8_t *)message,
            length,
            1000U
        );
    }
}


/* CRC-8: polynomial 0x07, initial value 0x00, final XOR 0x00 */
static uint8_t CRC8_Calculate(
    const uint8_t *data,
    uint16_t length)
{
    uint8_t crc = 0x00U;

    for (uint16_t i = 0U; i < length; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = (uint8_t)((crc << 1U) ^ 0x07U);
            }
            else
            {
                crc = (uint8_t)(crc << 1U);
            }
        }
    }

    return crc;
}


/* Calculate storage and validate block ranges */
static bool CalculateStorage(
    uint32_t *dataTotal,
    uint32_t *crcTotal,
    uint32_t *storageTotal)
{
    *dataTotal = 0U;
    *crcTotal = 0U;
    *storageTotal = 0U;

    for (uint32_t i = 0U; i < BLOCK_COUNT; i++)
    {
        uint32_t start = blocks[i].address;
        uint32_t size =
            (uint32_t)blocks[i].data_size + blocks[i].crc_size;
        uint32_t end = start + size;

        if ((blocks[i].data_size == 0U) ||
            (blocks[i].crc_size != CRC_SIZE) ||
            (blockData[i] == NULL) ||
            (end > EEPROM_CAPACITY))
        {
            return false;
        }

        /* Check for overlapping storage ranges */
        for (uint32_t j = 0U; j < i; j++)
        {
            uint32_t otherStart = blocks[j].address;
            uint32_t otherEnd = otherStart
                              + blocks[j].data_size
                              + blocks[j].crc_size;

            if ((start < otherEnd) && (otherStart < end))
            {
                return false;
            }
        }

        *dataTotal += blocks[i].data_size;
        *crcTotal += blocks[i].crc_size;
        *storageTotal += size;
    }

    return (*storageTotal <= EEPROM_CAPACITY);
}


/* Select lower or upper 256-byte section of the M24C04 */
static uint8_t EEPROM_DeviceAddress(uint16_t address)
{
    return (uint8_t)(
        EEPROM_BASE_ADDRESS | ((address >> 8U) & 0x01U)
    );
}


/*
 * Write bytes to EEPROM.
 * Split writes at 8-byte page boundaries and bank boundaries.
 */
static status_t EEPROM_WriteBytes(
    uint16_t address,
    const uint8_t *data,
    uint16_t length)
{
    uint16_t done = 0U;

    if ((data == NULL) ||
        (length == 0U) ||
        ((uint32_t)address + length > EEPROM_CAPACITY))
    {
        return STATUS_ERROR;
    }

    while (done < length)
    {
        uint16_t current = (uint16_t)(address + done);
        uint16_t pageOffset = current % EEPROM_PAGE_SIZE;
        uint16_t pageRemaining = EEPROM_PAGE_SIZE - pageOffset;
        uint16_t bankRemaining =
            EEPROM_BANK_SIZE - (current % EEPROM_BANK_SIZE);
        uint16_t remaining = length - done;
        uint16_t chunk = remaining;

        if (chunk > pageRemaining)
        {
            chunk = pageRemaining;
        }

        if (chunk > bankRemaining)
        {
            chunk = bankRemaining;
        }

        uint8_t txBuffer[EEPROM_PAGE_SIZE + 1U];

        /* First byte is the EEPROM's internal memory address */
        txBuffer[0] = (uint8_t)(current & 0xFFU);

        memcpy(&txBuffer[1], &data[done], chunk);

        LPI2C_DRV_MasterSetSlaveAddr(
            EEPROM_I2C,
            EEPROM_DeviceAddress(current),
            false
        );

        status_t status = LPI2C_DRV_MasterSendDataBlocking(
            EEPROM_I2C,
            txBuffer,
            (uint16_t)(chunk + 1U),
            true,
            EEPROM_TIMEOUT
        );

        if (status != STATUS_SUCCESS)
        {
            return status;
        }

        /* Wait for the EEPROM's internal write cycle */
        OSIF_TimeDelay(10U);

        done = (uint16_t)(done + chunk);
    }

    return STATUS_SUCCESS;
}


/*
 * Automatically calculate CRC, append it to data,
 * and write the combined buffer to EEPROM.
 */
static status_t EEPROM_WriteBlock(
    const EepromBlock *block,
    const uint8_t *data)
{
    uint8_t writeBuffer[257U];
    uint16_t totalLength;

    if ((block == NULL) ||
        (data == NULL) ||
        (block->data_size == 0U) ||
        (block->crc_size != CRC_SIZE))
    {
        return STATUS_ERROR;
    }

    totalLength =
        (uint16_t)(block->data_size + block->crc_size);

    if (totalLength > sizeof(writeBuffer))
    {
        return STATUS_ERROR;
    }

    /* Copy the original data into RAM buffer */
    memcpy(writeBuffer, data, block->data_size);

    /* Calculate CRC and append one byte */
    writeBuffer[block->data_size] =
        CRC8_Calculate(data, block->data_size);

    /* Write data + CRC to physical EEPROM */
    return EEPROM_WriteBytes(
        block->address,
        writeBuffer,
        totalLength
    );
}


int main(void)
{
    status_t status;
    uint32_t dataTotal;
    uint32_t crcTotal;
    uint32_t storageTotal;
    char message[160];

    status = CLOCK_DRV_Init(&clockMan1_InitConfig0);
    if (status != STATUS_SUCCESS)
    {
        while (1) {}
    }

    status = PINS_DRV_Init(
        NUM_OF_CONFIGURED_PINS0,
        g_pin_mux_InitConfigArr0
    );
    if (status != STATUS_SUCCESS)
    {
        while (1) {}
    }

    status = LPUART_DRV_Init(
        INST_LPUART_1,
        &uartState,
        &lpuart_0_InitConfig0
    );
    if (status != STATUS_SUCCESS)
    {
        while (1) {}
    }

    status = LPI2C_DRV_MasterInit(
        EEPROM_I2C,
        &lpi2c0_MasterConfig0,
        &lpi2cMasterState
    );
    if (status != STATUS_SUCCESS)
    {
        UART_Print("ERROR: LPI2C initialization failed\r\n");
        while (1) {}
    }

    UART_Print("\r\nCRC STORAGE OVERHEAD PROJECT\r\n");
    UART_Print("-----------------------------\r\n");

    /* Calculate storage before writing */
    if (!CalculateStorage(
            &dataTotal, &crcTotal, &storageTotal))
    {
        UART_Print(
            "CONFIGURATION ERROR: invalid block configuration\r\n"
        );
        while (1) {}
    }

    (void)snprintf(
        message, sizeof(message),
        "Configured blocks: %lu\r\n"
        "Data storage: %lu bytes\r\n"
        "CRC overhead: %lu bytes\r\n"
        "Total storage: %lu / %u bytes\r\n",
        (unsigned long)BLOCK_COUNT,
        (unsigned long)dataTotal,
        (unsigned long)crcTotal,
        (unsigned long)storageTotal,
        (unsigned int)EEPROM_CAPACITY
    );
    UART_Print(message);

    UART_Print("CAPACITY CHECK: PASS\r\n\r\n");

    /* Write every configured block with its CRC */
    for (uint32_t i = 0U; i < BLOCK_COUNT; i++)
    {
        status = EEPROM_WriteBlock(&blocks[i], blockData[i]);

        if (status != STATUS_SUCCESS)
        {
            (void)snprintf(
                message, sizeof(message),
                "%s: WRITE FAILED, status=%d\r\n",
                blocks[i].name, (int)status
            );
            UART_Print(message);
        }
        else
        {
            (void)snprintf(
                message, sizeof(message),
                "%s: Data + CRC written successfully\r\n",
                blocks[i].name
            );
            UART_Print(message);
        }
    }



    while (1) {}
}
