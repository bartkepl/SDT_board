/*
 * tmp117.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#include <tmp117.h>
#include <Utils.h>

static I2C_HandleTypeDef *i2c;
static uint16_t sTmp117TaskTimer = 0;
static TMP117_State_t eTmp117State = TMP117_STATE_IDLE;
static uint8_t ucTmp117RxBuffer[2];
static uint8_t ucTmp117TxBuffer[1];
static uint16_t usTmp117TaskWaitTimer = 0;

// State machine update timer (10ms)
#define TMP117_TASK_PERIOD_MS 10
#define TMP117_READ_PERIOD_MS 1000

TMP117_Data_t g_tmp117 = {0};

//------------------------------------------------------------------//
// Private functions
//------------------------------------------------------------------//

static void TMP117_RequestRead(uint8_t ucRegister)
{
    ucTmp117TxBuffer[0] = ucRegister;
    
    if (HAL_I2C_Master_Transmit_DMA(i2c, TMP117_ADDR, ucTmp117TxBuffer, 1) != HAL_OK)
    {
        eTmp117State = TMP117_STATE_ERROR;
    }
}

static void TMP117_RequestReadData(void)
{
    if (HAL_I2C_Master_Receive_DMA(i2c, TMP117_ADDR, ucTmp117RxBuffer, 2) != HAL_OK)
    {
        eTmp117State = TMP117_STATE_ERROR;
    }
}

static void TMP117_ProcessTemp(void)
{
    int16_t raw = ((int16_t)ucTmp117RxBuffer[0] << 8) | ucTmp117RxBuffer[1];
    g_tmp117.fTemp = raw * 0.0078125f;
    g_tmp117.ucNewDataFlag = 1;
}

static void TMP117_ProcessId(void)
{
    g_tmp117.usId = (ucTmp117RxBuffer[0] << 8) | ucTmp117RxBuffer[1];
}

static void TMP117_ProcessConfig(void)
{
    g_tmp117.usConfigRegister = (ucTmp117RxBuffer[0] << 8) | ucTmp117RxBuffer[1];
}

//------------------------------------------------------------------//
// Public functions
//------------------------------------------------------------------//

void TMP117_Init(I2C_HandleTypeDef *hi2c)
{
    i2c = hi2c;
    
    g_tmp117.ucValidFlag = 0;
    g_tmp117.ucNewDataFlag = 0;
    g_tmp117.ucInitializedFlag = 1;
    g_tmp117.fTemp = 0;
    g_tmp117.usId = 0;
    g_tmp117.usConfigRegister = 0;
    
    eTmp117State = TMP117_STATE_REQ_TEMP;
    SysTimZeroTimer1ms_u16(&usTmp117TaskWaitTimer);
}

void TMP117_Task(void)
{
    // State machine runs every 10ms
    if (!SysTimTestTimer1ms_u16(&sTmp117TaskTimer, TMP117_TASK_PERIOD_MS))
        return;

    // Wait timer logic - delay between read operations
    if (usTmp117TaskWaitTimer > 0)
    {
        usTmp117TaskWaitTimer--;
        return;
    }

    switch (eTmp117State)
    {
        case TMP117_STATE_IDLE:
            // Periodic read every 1000ms
            if (SysTimTestTimer1ms_u16(&sTmp117TaskTimer, TMP117_READ_PERIOD_MS))
            {
                eTmp117State = TMP117_STATE_REQ_TEMP;
            }
            break;

        case TMP117_STATE_REQ_TEMP:
            // Request temperature register
            TMP117_RequestRead(TMP117_REG_TEMP);
            eTmp117State = TMP117_STATE_REC_TEMP;
            usTmp117TaskWaitTimer = 2; // Wait ~20ms for I2C
            break;

        case TMP117_STATE_REC_TEMP:
            // Read temperature data (handled by DMA + callback)
            TMP117_RequestReadData();
            eTmp117State = TMP117_STATE_REQ_ID;
            usTmp117TaskWaitTimer = 2;
            break;

        case TMP117_STATE_REQ_ID:
            // Process temperature and request ID
            TMP117_ProcessTemp();
            TMP117_RequestRead(TMP117_REG_ID);
            eTmp117State = TMP117_STATE_REC_ID;
            usTmp117TaskWaitTimer = 2;
            break;

        case TMP117_STATE_REC_ID:
            // Read ID data
            TMP117_RequestReadData();
            eTmp117State = TMP117_STATE_REQ_CONFIG;
            usTmp117TaskWaitTimer = 2;
            break;

        case TMP117_STATE_REQ_CONFIG:
            // Process ID and request config
            TMP117_ProcessId();
            TMP117_RequestRead(TMP117_REG_CONFIG);
            eTmp117State = TMP117_STATE_REC_CONFIG;
            usTmp117TaskWaitTimer = 2;
            break;

        case TMP117_STATE_REC_CONFIG:
            // Read config data
            TMP117_RequestReadData();
            eTmp117State = TMP117_STATE_IDLE;
            usTmp117TaskWaitTimer = 2;
            break;

        case TMP117_STATE_ERROR:
            // Error state - reset after a while
            usTmp117TaskWaitTimer = 100; // Wait 1000ms before retry
            eTmp117State = TMP117_STATE_REQ_TEMP;
            break;

        default:
            eTmp117State = TMP117_STATE_ERROR;
            break;
    }
}

void TMP117_I2C_Complete_Callback(void)
{
    // Process received data based on current state
    if (eTmp117State == TMP117_STATE_REC_TEMP)
    {
        TMP117_ProcessTemp();
    }
    else if (eTmp117State == TMP117_STATE_REC_ID)
    {
        TMP117_ProcessId();
    }
    else if (eTmp117State == TMP117_STATE_REC_CONFIG)
    {
        TMP117_ProcessConfig();
        g_tmp117.ucValidFlag = 1; // Mark data as valid after all reads complete
    }
}

void TMP117_I2C_Error_Callback(void)
{
    eTmp117State = TMP117_STATE_ERROR;
}
