/*
 * sht45.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#include <sht45.h>
#include <Utils.h>

static I2C_HandleTypeDef *i2c;
static uint16_t sSht45TaskTimer = 0;
static uint16_t sSht45ReadTimer = 0;
static SHT45_State_t eSht45State = SHT45_STATE_IDLE;
static uint16_t usSht45WaitTimer = 0;
static uint8_t ucSht45RxBuffer[6];
static uint8_t ucSht45TxBuffer[1];
static volatile uint8_t ucSht45TxComplete = 0;
static volatile uint8_t ucSht45RxComplete = 0;
static volatile uint8_t ucSht45Error = 0;

// State machine timer period (10ms)
#define SHT45_TASK_PERIOD_MS 10
#define SHT45_READ_PERIOD_MS 1000
#define SHT45_WAIT_TIMEOUT_MS 100  // 100ms timeout for DMA operations

SHT45_Data_t g_sht45 = {0};
static uint16_t usSht45WaitTimeout = 0;  // Timeout counter for wait states

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

static void SHT45_SendCommand(uint8_t cmd)
{
    ucSht45TxBuffer[0] = cmd;
    ucSht45TxComplete = 0;
    ucSht45RxComplete = 0;
    ucSht45Error = 0;
    
    if (HAL_I2C_Master_Transmit_DMA(i2c, SHT45_ADDR, ucSht45TxBuffer, 1) != HAL_OK) {
        eSht45State = SHT45_STATE_ERROR;
    }
}

static void SHT45_ReceiveData(uint8_t ucLen)
{
    ucSht45RxComplete = 0;
    ucSht45Error = 0;
    
    if (HAL_I2C_Master_Receive_DMA(i2c, SHT45_ADDR, ucSht45RxBuffer, ucLen) != HAL_OK)
    {
        eSht45State = SHT45_STATE_ERROR;
    }
}

static void SHT45_ProcessMeasure(void)
{
    // Check CRC for temperature
    if (crc8(ucSht45RxBuffer, 2) != ucSht45RxBuffer[2])
    {
        g_sht45.ucValidFlag = 0;
        return;
    }
    
    // Check CRC for humidity
    if (crc8(ucSht45RxBuffer + 3, 2) != ucSht45RxBuffer[5])
    {
        g_sht45.ucValidFlag = 0;
        return;
    }

    uint16_t rawT = (ucSht45RxBuffer[0] << 8) | ucSht45RxBuffer[1];
    uint16_t rawH = (ucSht45RxBuffer[3] << 8) | ucSht45RxBuffer[4];

    g_sht45.fTemp = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    g_sht45.fHum = -6.0f + 125.0f * ((float)rawH / 65535.0f);
    
    g_sht45.ucValidFlag = 1;
    g_sht45.ucNewDataFlag = 1;
}

static void SHT45_ProcessSerial(void)
{
    // Check CRC
    if (crc8(ucSht45RxBuffer, 2) != ucSht45RxBuffer[2])
        return;
    if (crc8(ucSht45RxBuffer + 3, 2) != ucSht45RxBuffer[5])
        return;

    g_sht45.uSerialNumber = (ucSht45RxBuffer[0] << 24) | (ucSht45RxBuffer[1] << 16) | 
                            (ucSht45RxBuffer[3] << 8) | ucSht45RxBuffer[4];
    
    g_sht45.ucValidFlag = 1;
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
    g_sht45.uSerialNumber = 0;
    g_sht45.eMeasPrecision = SHT45_PRECISION_HIGH;
    g_sht45.eHeaterMode = SHT45_HEATER_20MW_1S;
    
    ucSht45TxComplete = 0;
    ucSht45RxComplete = 0;
    ucSht45Error = 0;
    
    eSht45State = SHT45_STATE_IDLE;
    SysTimZeroTimer1ms_u16(&sSht45TaskTimer);
    SysTimZeroTimer1ms_u16(&sSht45ReadTimer);
    SysTimZeroTimer1ms_u16(&usSht45WaitTimer);
}

void SHT45_Task(void)
{
    // State machine runs every 10ms
    if (!SysTimTestTimer1ms_u16(&sSht45TaskTimer, SHT45_TASK_PERIOD_MS))
        return;

    // Wait timer logic
    if (usSht45WaitTimer > 0)
    {
        usSht45WaitTimer--;
        return;
    }

    switch (eSht45State)
    {
        case SHT45_STATE_IDLE:
            // Periodic measurement every 1000ms
            if (SysTimTestTimer1ms_u16(&sSht45ReadTimer, SHT45_READ_PERIOD_MS))
            {
                eSht45State = SHT45_STATE_REQ_MEASURE;
            }
            break;

        case SHT45_STATE_REQ_MEASURE:
            // Request measurement
            SHT45_SendCommand(g_sht45.eMeasPrecision);
            eSht45State = SHT45_STATE_WAIT_TX_MEASURE;
            break;

        case SHT45_STATE_WAIT_TX_MEASURE:
        {
            volatile uint8_t tx_complete = ucSht45TxComplete;
            volatile uint8_t error = ucSht45Error;
            
            if (tx_complete)
            {
                ucSht45TxComplete = 0;  // Reset flag
                eSht45State = SHT45_STATE_REC_MEASURE;
                usSht45WaitTimer = 2; // Wait for conversion ~20ms
            }
            else if (error)
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;
        }

        case SHT45_STATE_REC_MEASURE:
            // Receive measurement data
            SHT45_ReceiveData(6);
            eSht45State = SHT45_STATE_WAIT_RX_MEASURE;
            break;

        case SHT45_STATE_WAIT_RX_MEASURE:
        {
            volatile uint8_t rx_complete = ucSht45RxComplete;
            volatile uint8_t error = ucSht45Error;
            
            if (rx_complete)
            {
                ucSht45RxComplete = 0;  // Reset flag
                eSht45State = SHT45_STATE_PROC_MEASURE;
            }
            else if (error)
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;
        }

        case SHT45_STATE_PROC_MEASURE:
        	// Process measurement result
        	SHT45_ProcessMeasure();
			eSht45State = SHT45_STATE_REQ_SERIAL;
			usSht45WaitTimer = 2;
			break;

        case SHT45_STATE_REQ_SERIAL:
            // Request serial number
            SHT45_SendCommand(SHT45_CMD_READ_SERIAL_NUMB);
            eSht45State = SHT45_STATE_WAIT_TX_SERIAL;
            break;

        case SHT45_STATE_WAIT_TX_SERIAL:
        {
            volatile uint8_t tx_complete = ucSht45TxComplete;
            volatile uint8_t error = ucSht45Error;
            
            if (tx_complete)
            {
                ucSht45TxComplete = 0;  // Reset flag
                eSht45State = SHT45_STATE_REC_SERIAL;
                usSht45WaitTimer = 2;
            }
            else if (error)
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;
        }

        case SHT45_STATE_REC_SERIAL:
            // Receive serial number
            SHT45_ReceiveData(6);
            eSht45State = SHT45_STATE_WAIT_RX_SERIAL;
            break;

        case SHT45_STATE_WAIT_RX_SERIAL:
        {
            volatile uint8_t rx_complete = ucSht45RxComplete;
            volatile uint8_t error = ucSht45Error;
            
            if (rx_complete)
            {
                ucSht45RxComplete = 0;  // Reset flag
                eSht45State = SHT45_STATE_PROC_SERIAL;
            }
            else if (error)
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;
        }

		case SHT45_STATE_PROC_SERIAL:
			// Receive serial number
			SHT45_ProcessSerial();
			eSht45State = SHT45_STATE_IDLE;
			break;

        case SHT45_STATE_REQ_HEATER:
            // Request heater operation
            SHT45_SendCommand(g_sht45.eHeaterMode);
            eSht45State = SHT45_STATE_WAIT_TX_HEATER;
            g_sht45.ucHeaterActivationFlag = 0;
            break;

        case SHT45_STATE_WAIT_TX_HEATER:
        {
            volatile uint8_t tx_complete = ucSht45TxComplete;
            volatile uint8_t error = ucSht45Error;
            
            if (tx_complete)
            {
                ucSht45TxComplete = 0;  // Reset flag
                eSht45State = SHT45_STATE_REC_HEATER;
                usSht45WaitTimer = 120; // Wait for heater operation (1s + margin)
            }
            else if (error)
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;
        }

        case SHT45_STATE_REC_HEATER:
            // Receive heater result
            SHT45_ReceiveData(6);
            eSht45State = SHT45_STATE_WAIT_RX_HEATER;
            break;

        case SHT45_STATE_WAIT_RX_HEATER:
        {
            volatile uint8_t rx_complete = ucSht45RxComplete;
            volatile uint8_t error = ucSht45Error;
            
            if (rx_complete)
            {
                ucSht45RxComplete = 0;  // Reset flag
                eSht45State = SHT45_STATE_IDLE;
            }
            else if (error)
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;
        }

        case SHT45_STATE_REQ_SOFTRESET:
            // Request soft reset
            SHT45_SendCommand(SHT45_CMD_SOFT_RESET);
            eSht45State = SHT45_STATE_WAIT_TX_SOFTRESET;
            g_sht45.ucSoftResetFlag = 0;
            break;

        case SHT45_STATE_WAIT_TX_SOFTRESET:
        {
            volatile uint8_t tx_complete = ucSht45TxComplete;
            volatile uint8_t error = ucSht45Error;
            
            if (tx_complete)
            {
                ucSht45TxComplete = 0;  // Reset flag
                eSht45State = SHT45_STATE_WAIT_SOFTRESET;
                usSht45WaitTimer = 50; // Wait 500ms for reset
            }
            else if (error)
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;
        }

        case SHT45_STATE_WAIT_SOFTRESET:
            // Send soft reset restart command
            SHT45_SendCommand(SHT45_CMD_SOFT_RESET_RESTART);
            eSht45State = SHT45_STATE_WAIT_TX_SOFTRESET_RESTART;
            break;

        case SHT45_STATE_WAIT_TX_SOFTRESET_RESTART:
        {
            volatile uint8_t tx_complete = ucSht45TxComplete;
            volatile uint8_t error = ucSht45Error;
            
            if (tx_complete)
            {
                ucSht45TxComplete = 0;  // Reset flag
                eSht45State = SHT45_STATE_WAIT_SOFTRESET_COMPLETE;
                usSht45WaitTimer = 50; // Wait for restart
            }
            else if (error)
            {
                eSht45State = SHT45_STATE_ERROR;
            }
            break;
        }

        case SHT45_STATE_WAIT_SOFTRESET_COMPLETE:
            // Soft reset complete
            eSht45State = SHT45_STATE_IDLE;
            break;

        case SHT45_STATE_ERROR:
            // Error state - reset after retry timeout
            usSht45WaitTimer = 100; // Wait 1000ms before retry
            eSht45State = SHT45_STATE_REQ_MEASURE;
            break;

        default:
            eSht45State = SHT45_STATE_ERROR;
            break;
    }

    // Check for pending requests
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
    ucSht45TxComplete = 1;
}

void SHT45_I2C_RxComplete_Callback(void)
{
    ucSht45RxComplete = 1;
}

void SHT45_I2C_Error_Callback(void)
{
    ucSht45Error = 1;
    eSht45State = SHT45_STATE_ERROR;
}

void SHT45_RequestHeater(SHT45_HeaterMode_t eMode)
{
    g_sht45.eHeaterMode = eMode;
    g_sht45.ucHeaterActivationFlag = 1;
}

void SHT45_RequestSoftReset(void)
{
    g_sht45.ucSoftResetFlag = 1;
}

void SHT45_SetMeasurementPrecision(SHT45_Precision_t ePrecision)
{
    g_sht45.eMeasPrecision = ePrecision;
}
