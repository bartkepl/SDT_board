/*
 * sht45.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#include <sht45.h>
#include <sensor.h>
#include <Utils.h>

static I2C_HandleTypeDef *i2c;
static uint16_t sSht45TaskTimer = 0;
static uint16_t sSht45ReadTimer = 0;
static SHT45_State_t eSht45State = SHT45_STATE_IDLE;
static uint16_t usSht45WaitTimer = 0;
static uint16_t usSht45DmaTimeout = 0;
static uint8_t ucSht45RxBuffer[6];
static uint8_t ucSht45TxBuffer[1];
static volatile uint8_t ucSht45TxComplete = 0;
static volatile uint8_t ucSht45RxComplete = 0;
static volatile uint8_t ucSht45Error = 0;

#define SHT45_TASK_PERIOD_MS        10
#define SHT45_DMA_TIMEOUT_TICKS     (100 / SHT45_TASK_PERIOD_MS)   // 10 ticks = 100ms
#define SHT45_WAIT_MEASURE_TICKS    (20   / SHT45_TASK_PERIOD_MS)  // 2  ticks = 20ms
#define SHT45_WAIT_HEATER_TICKS     (1200 / SHT45_TASK_PERIOD_MS)  // 120 ticks = 1200ms
#define SHT45_WAIT_SOFTRESET_TICKS  (500  / SHT45_TASK_PERIOD_MS)  // 50 ticks = 500ms
#define SHT45_WAIT_ERROR_TICKS      (1000 / SHT45_TASK_PERIOD_MS)  // 100 ticks = 1000ms

SHT45_Data_t g_sht45 = {0};

SHT45_Config_t g_sht45_config = {
    .eMeasPrecision = SHT45_PRECISION_HIGH,
    .eHeaterMode = SHT45_HEATER_20MW_1S,
    .readPeriodMs = 1000,
    .averageCount = 1,
};

//------------------------------------------------------------------//
// Private functions
//------------------------------------------------------------------//

static uint8_t crc8(uint8_t *data, int len)
{
    uint8_t crc = 0xFF;

    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// Returns false if I2C bus is busy by the other sensor (DUAL mode)
static bool SHT45_SendCommand(uint8_t cmd)
{
    if (g_i2c_active_sensor != I2C_SENSOR_NONE && g_i2c_active_sensor != I2C_SENSOR_SHT45)
        return false;

    g_i2c_active_sensor = I2C_SENSOR_SHT45;
    ucSht45TxBuffer[0] = cmd;
    ucSht45TxComplete = 0;
    ucSht45RxComplete = 0;
    ucSht45Error = 0;

    if (HAL_I2C_Master_Transmit_DMA(i2c, SHT45_ADDR, ucSht45TxBuffer, 1) != HAL_OK) {
        g_i2c_active_sensor = I2C_SENSOR_NONE;
        eSht45State = SHT45_STATE_ERROR;
        return false;
    }
    return true;
}

static bool SHT45_ReceiveData(uint8_t ucLen)
{
    if (g_i2c_active_sensor != I2C_SENSOR_NONE && g_i2c_active_sensor != I2C_SENSOR_SHT45)
        return false;

    g_i2c_active_sensor = I2C_SENSOR_SHT45;
    ucSht45RxComplete = 0;
    ucSht45Error = 0;

    if (HAL_I2C_Master_Receive_DMA(i2c, SHT45_ADDR, ucSht45RxBuffer, ucLen) != HAL_OK)
    {
        g_i2c_active_sensor = I2C_SENSOR_NONE;
        eSht45State = SHT45_STATE_ERROR;
        return false;
    }
    return true;
}

static void SHT45_ProcessMeasure(void)
{
    if (crc8(ucSht45RxBuffer, 2) != ucSht45RxBuffer[2])
    {
        g_sht45.ucValidFlag = 0;
        return;
    }

    if (crc8(ucSht45RxBuffer + 3, 2) != ucSht45RxBuffer[5])
    {
        g_sht45.ucValidFlag = 0;
        return;
    }

    uint16_t rawT = (ucSht45RxBuffer[0] << 8) | ucSht45RxBuffer[1];
    uint16_t rawH = (ucSht45RxBuffer[3] << 8) | ucSht45RxBuffer[4];

    float temp = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    float hum = -6.0f + 125.0f * ((float)rawH / 65535.0f);

    uint8_t avgCount = g_sht45_config.averageCount;

    if (avgCount == 0)
        avgCount = 1;

    if (avgCount == 1)
    {
        g_sht45.fTemp = temp;
        g_sht45.fHum = hum;
        g_sht45.ucValidFlag = 1;
        g_sht45.ucNewDataFlag = 1;
        return;
    }

    g_sht45.fTempAcc += temp;
    g_sht45.fHumAcc += hum;
    g_sht45.ucAverageSampleCnt++;

    if (g_sht45.ucAverageSampleCnt < avgCount)
        return;

    g_sht45.fTemp = g_sht45.fTempAcc / (float)g_sht45.ucAverageSampleCnt;
    g_sht45.fHum  = g_sht45.fHumAcc  / (float)g_sht45.ucAverageSampleCnt;

    g_sht45.fTempAcc = 0.0f;
    g_sht45.fHumAcc  = 0.0f;
    g_sht45.ucAverageSampleCnt = 0;

    g_sht45.ucValidFlag = 1;
    g_sht45.ucNewDataFlag = 1;
}

static void SHT45_ProcessSerial(void)
{
    if (crc8(ucSht45RxBuffer, 2) != ucSht45RxBuffer[2])
        return;
    if (crc8(ucSht45RxBuffer + 3, 2) != ucSht45RxBuffer[5])
        return;

    g_sht45.uSerialNumber = ((uint32_t)ucSht45RxBuffer[0] << 24) | ((uint32_t)ucSht45RxBuffer[1] << 16) |
                            ((uint32_t)ucSht45RxBuffer[3] << 8)  |  (uint32_t)ucSht45RxBuffer[4];
}

//------------------------------------------------------------------//
// DMA timeout helper — call in every WAIT_TX/WAIT_RX state
// Returns true if timeout expired (caller should go to ERROR state)
//------------------------------------------------------------------//
static bool SHT45_CheckDmaTimeout(void)
{
    if (++usSht45DmaTimeout >= SHT45_DMA_TIMEOUT_TICKS)
    {
        usSht45DmaTimeout = 0;
        g_i2c_active_sensor = I2C_SENSOR_NONE;
        return true;
    }
    return false;
}

//------------------------------------------------------------------//
// Public functions
//------------------------------------------------------------------//

void SHT45_Init(I2C_HandleTypeDef *hi2c)
{
    i2c = hi2c;

    g_sht45.ucValidFlag = 0;
    g_sht45.ucNewDataFlag = 0;
    g_sht45.ucInitializedFlag = 1;
    g_sht45.fTemp = 0;
    g_sht45.fHum = 0;
    g_sht45.fTempAcc = 0.0f;
    g_sht45.fHumAcc = 0.0f;
    g_sht45.ucAverageSampleCnt = 0;
    g_sht45.uSerialNumber = 0;
    g_sht45.ucHeaterActivationFlag = 0;
    g_sht45.ucSoftResetFlag = 0;

    ucSht45TxComplete = 0;
    ucSht45RxComplete = 0;
    ucSht45Error = 0;

    eSht45State = SHT45_STATE_IDLE;
    usSht45WaitTimer = 0;
    usSht45DmaTimeout = 0;
    SysTimZeroTimer1ms_u16(&sSht45TaskTimer);
    SysTimZeroTimer1ms_u16(&sSht45ReadTimer);
}

void SHT45_Task(void)
{
    if (!SysTimTestTimer1ms_u16(&sSht45TaskTimer, SHT45_TASK_PERIOD_MS))
        return;

    if (usSht45WaitTimer > 0)
    {
        usSht45WaitTimer--;
        return;
    }

    switch (eSht45State)
    {
        case SHT45_STATE_IDLE:
            if (SysTimTestTimer1ms_u16(&sSht45ReadTimer, g_sht45_config.readPeriodMs))
            {
                eSht45State = SHT45_STATE_REQ_MEASURE;
            }
            break;

        case SHT45_STATE_REQ_MEASURE:
            if (!SHT45_SendCommand(g_sht45_config.eMeasPrecision))
                break; // I2C busy, retry next tick
            usSht45DmaTimeout = 0;
            eSht45State = SHT45_STATE_WAIT_TX_MEASURE;
            break;

        case SHT45_STATE_WAIT_TX_MEASURE:
            if (ucSht45TxComplete)
            {
                ucSht45TxComplete = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_REC_MEASURE;
                usSht45WaitTimer = SHT45_WAIT_MEASURE_TICKS;
            }
            else if (ucSht45Error)
            {
                ucSht45Error = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_ERROR;
            }
            else if (SHT45_CheckDmaTimeout())
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;

        case SHT45_STATE_REC_MEASURE:
            if (!SHT45_ReceiveData(6))
                break;
            usSht45DmaTimeout = 0;
            eSht45State = SHT45_STATE_WAIT_RX_MEASURE;
            break;

        case SHT45_STATE_WAIT_RX_MEASURE:
            if (ucSht45RxComplete)
            {
                ucSht45RxComplete = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_PROC_MEASURE;
            }
            else if (ucSht45Error)
            {
                ucSht45Error = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_ERROR;
            }
            else if (SHT45_CheckDmaTimeout())
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;

        case SHT45_STATE_PROC_MEASURE:
            SHT45_ProcessMeasure();
            eSht45State = SHT45_STATE_REQ_SERIAL;
            usSht45WaitTimer = SHT45_WAIT_MEASURE_TICKS;
            break;

        case SHT45_STATE_REQ_SERIAL:
            if (!SHT45_SendCommand(SHT45_CMD_READ_SERIAL_NUMB))
                break;
            usSht45DmaTimeout = 0;
            eSht45State = SHT45_STATE_WAIT_TX_SERIAL;
            break;

        case SHT45_STATE_WAIT_TX_SERIAL:
            if (ucSht45TxComplete)
            {
                ucSht45TxComplete = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_REC_SERIAL;
                usSht45WaitTimer = SHT45_WAIT_MEASURE_TICKS;
            }
            else if (ucSht45Error)
            {
                ucSht45Error = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_ERROR;
            }
            else if (SHT45_CheckDmaTimeout())
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;

        case SHT45_STATE_REC_SERIAL:
            if (!SHT45_ReceiveData(6))
                break;
            usSht45DmaTimeout = 0;
            eSht45State = SHT45_STATE_WAIT_RX_SERIAL;
            break;

        case SHT45_STATE_WAIT_RX_SERIAL:
            if (ucSht45RxComplete)
            {
                ucSht45RxComplete = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_PROC_SERIAL;
            }
            else if (ucSht45Error)
            {
                ucSht45Error = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_ERROR;
            }
            else if (SHT45_CheckDmaTimeout())
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;

        case SHT45_STATE_PROC_SERIAL:
            SHT45_ProcessSerial();
            eSht45State = SHT45_STATE_IDLE;
            break;

        case SHT45_STATE_REQ_HEATER:
            if (!SHT45_SendCommand(g_sht45_config.eHeaterMode))
                break;
            g_sht45.ucHeaterActivationFlag = 0;
            usSht45DmaTimeout = 0;
            eSht45State = SHT45_STATE_WAIT_TX_HEATER;
            break;

        case SHT45_STATE_WAIT_TX_HEATER:
            if (ucSht45TxComplete)
            {
                ucSht45TxComplete = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_REC_HEATER;
                usSht45WaitTimer = SHT45_WAIT_HEATER_TICKS;
            }
            else if (ucSht45Error)
            {
                ucSht45Error = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_ERROR;
            }
            else if (SHT45_CheckDmaTimeout())
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;

        case SHT45_STATE_REC_HEATER:
            if (!SHT45_ReceiveData(6))
                break;
            usSht45DmaTimeout = 0;
            eSht45State = SHT45_STATE_WAIT_RX_HEATER;
            break;

        case SHT45_STATE_WAIT_RX_HEATER:
            if (ucSht45RxComplete)
            {
                ucSht45RxComplete = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_IDLE;
            }
            else if (ucSht45Error)
            {
                ucSht45Error = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_ERROR;
            }
            else if (SHT45_CheckDmaTimeout())
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;

        case SHT45_STATE_REQ_SOFTRESET:
            if (!SHT45_SendCommand(SHT45_CMD_SOFT_RESET))
                break;
            g_sht45.ucSoftResetFlag = 0;
            usSht45DmaTimeout = 0;
            eSht45State = SHT45_STATE_WAIT_TX_SOFTRESET;
            break;

        case SHT45_STATE_WAIT_TX_SOFTRESET:
            if (ucSht45TxComplete)
            {
                ucSht45TxComplete = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_WAIT_SOFTRESET;
                usSht45WaitTimer = SHT45_WAIT_SOFTRESET_TICKS;
            }
            else if (ucSht45Error)
            {
                ucSht45Error = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_ERROR;
            }
            else if (SHT45_CheckDmaTimeout())
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;

        case SHT45_STATE_WAIT_SOFTRESET:
            if (!SHT45_SendCommand(SHT45_CMD_SOFT_RESET_RESTART))
                break;
            usSht45DmaTimeout = 0;
            eSht45State = SHT45_STATE_WAIT_TX_SOFTRESET_RESTART;
            break;

        case SHT45_STATE_WAIT_TX_SOFTRESET_RESTART:
            if (ucSht45TxComplete)
            {
                ucSht45TxComplete = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_WAIT_SOFTRESET_COMPLETE;
                usSht45WaitTimer = SHT45_WAIT_SOFTRESET_TICKS;
            }
            else if (ucSht45Error)
            {
                ucSht45Error = 0;
                usSht45DmaTimeout = 0;
                eSht45State = SHT45_STATE_ERROR;
            }
            else if (SHT45_CheckDmaTimeout())
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;

        case SHT45_STATE_WAIT_SOFTRESET_COMPLETE:
            eSht45State = SHT45_STATE_IDLE;
            break;

        case SHT45_STATE_ERROR:
            usSht45WaitTimer = SHT45_WAIT_ERROR_TICKS;
            eSht45State = SHT45_STATE_REQ_MEASURE;
            break;

        default:
            eSht45State = SHT45_STATE_ERROR;
            break;
    }

    if (g_sht45.ucHeaterActivationFlag && eSht45State == SHT45_STATE_IDLE)
    {
        eSht45State = SHT45_STATE_REQ_HEATER;
    }

    if (g_sht45.ucSoftResetFlag && eSht45State == SHT45_STATE_IDLE)
    {
        eSht45State = SHT45_STATE_REQ_SOFTRESET;
    }
}

void SHT45_I2C_TxComplete_Callback(void)
{
    g_i2c_active_sensor = I2C_SENSOR_NONE;
    ucSht45TxComplete = 1;
}

void SHT45_I2C_RxComplete_Callback(void)
{
    g_i2c_active_sensor = I2C_SENSOR_NONE;
    ucSht45RxComplete = 1;
}

void SHT45_I2C_Error_Callback(void)
{
    g_i2c_active_sensor = I2C_SENSOR_NONE;
    ucSht45Error = 1;
    eSht45State = SHT45_STATE_ERROR;
}

void SHT45_RequestHeater(SHT45_HeaterMode_t eMode)
{
    g_sht45_config.eHeaterMode = eMode;
    g_sht45.ucHeaterActivationFlag = 1;
}

void SHT45_RequestSoftReset(void)
{
    g_sht45.ucSoftResetFlag = 1;
}

void SHT45_SetReadPeriod(uint16_t periodMs)
{
    g_sht45_config.readPeriodMs = periodMs;
    SysTimZeroTimer1ms_u16(&sSht45ReadTimer);
}

uint16_t SHT45_GetReadPeriod(void)
{
    return g_sht45_config.readPeriodMs;
}

void SHT45_SetAverageCount(uint8_t count)
{
    if (count < 1)
        count = 1;

    g_sht45_config.averageCount = count;

    g_sht45.fTempAcc = 0.0f;
    g_sht45.fHumAcc = 0.0f;
    g_sht45.ucAverageSampleCnt = 0;
}

uint8_t SHT45_GetAverageCount(void)
{
    return g_sht45_config.averageCount;
}

void SHT45_SetPrecision(SHT45_Precision_t precision)
{
    g_sht45_config.eMeasPrecision = precision;
}

SHT45_Precision_t SHT45_GetPrecision(void)
{
    return g_sht45_config.eMeasPrecision;
}

void SHT45_SetMeasurementPrecision(SHT45_Precision_t ePrecision)
{
    SHT45_SetPrecision(ePrecision);
}
