/*
 * tmp117.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#include <tmp117.h>
#include <sensor.h>
#include <Utils.h>

static I2C_HandleTypeDef *i2c;
static uint16_t sTmp117TaskTimer = 0;
static uint16_t sTmp117ReadTimer = 0;
static TMP117_State_t eTmp117State = TMP117_STATE_IDLE;
static uint8_t ucTmp117RxBuffer[2];
static uint8_t ucTmp117TxBuffer[1];
static uint16_t usTmp117TaskWaitTimer = 0;
static uint16_t usTmp117DmaTimeout = 0;
static volatile uint8_t ucTmp117TxComplete = 0;
static volatile uint8_t ucTmp117RxComplete = 0;
static volatile uint8_t ucTmp117Error = 0;

#define TMP117_TASK_PERIOD_MS       10
#define TMP117_READ_PERIOD_MS       1000
#define TMP117_DMA_TIMEOUT_TICKS    (100 / TMP117_TASK_PERIOD_MS)  // 10 ticks = 100ms
#define TMP117_WAIT_I2C_TICKS       (20  / TMP117_TASK_PERIOD_MS)  // 2  ticks = 20ms
#define TMP117_WAIT_ERROR_TICKS     (1000 / TMP117_TASK_PERIOD_MS) // 100 ticks = 1000ms

TMP117_Data_t g_tmp117 = {0};

//------------------------------------------------------------------//
// Private functions
//------------------------------------------------------------------//

// Returns false if I2C bus is busy by the other sensor (DUAL mode)
static bool TMP117_RequestRead(uint8_t ucRegister)
{
    if (g_i2c_active_sensor != I2C_SENSOR_NONE && g_i2c_active_sensor != I2C_SENSOR_TMP117)
        return false;

    g_i2c_active_sensor = I2C_SENSOR_TMP117;
    ucTmp117TxBuffer[0] = ucRegister;
    ucTmp117TxComplete = 0;
    ucTmp117RxComplete = 0;
    ucTmp117Error = 0;

    if (HAL_I2C_Master_Transmit_DMA(i2c, TMP117_ADDR, ucTmp117TxBuffer, 1) != HAL_OK)
    {
        g_i2c_active_sensor = I2C_SENSOR_NONE;
        eTmp117State = TMP117_STATE_ERROR;
        return false;
    }
    return true;
}

static bool TMP117_RequestReadData(void)
{
    if (g_i2c_active_sensor != I2C_SENSOR_NONE && g_i2c_active_sensor != I2C_SENSOR_TMP117)
        return false;

    g_i2c_active_sensor = I2C_SENSOR_TMP117;
    ucTmp117TxComplete = 0;
    ucTmp117RxComplete = 0;
    ucTmp117Error = 0;

    if (HAL_I2C_Master_Receive_DMA(i2c, TMP117_ADDR, ucTmp117RxBuffer, 2) != HAL_OK)
    {
        g_i2c_active_sensor = I2C_SENSOR_NONE;
        eTmp117State = TMP117_STATE_ERROR;
        return false;
    }
    return true;
}

static void TMP117_ProcessTemp(void)
{
    int16_t raw = ((int16_t)ucTmp117RxBuffer[0] << 8) | ucTmp117RxBuffer[1];
    g_tmp117.fTemp = raw * 0.0078125f;
    g_tmp117.ucValidFlag = 1;
    g_tmp117.ucNewDataFlag = 1;
}

static void TMP117_ProcessId(void)
{
    g_tmp117.usId = ((uint16_t)ucTmp117RxBuffer[0] << 8) | ucTmp117RxBuffer[1];
}

static void TMP117_ProcessConfig(void)
{
    g_tmp117.usConfigRegister = ((uint16_t)ucTmp117RxBuffer[0] << 8) | ucTmp117RxBuffer[1];
}

static bool TMP117_CheckDmaTimeout(void)
{
    if (++usTmp117DmaTimeout >= TMP117_DMA_TIMEOUT_TICKS)
    {
        usTmp117DmaTimeout = 0;
        g_i2c_active_sensor = I2C_SENSOR_NONE;
        return true;
    }
    return false;
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

    ucTmp117TxComplete = 0;
    ucTmp117RxComplete = 0;
    ucTmp117Error = 0;

    eTmp117State = TMP117_STATE_IDLE;
    usTmp117TaskWaitTimer = 0;
    usTmp117DmaTimeout = 0;
    SysTimZeroTimer1ms_u16(&sTmp117TaskTimer);
    SysTimZeroTimer1ms_u16(&sTmp117ReadTimer);
}

void TMP117_Task(void)
{
    if (!SysTimTestTimer1ms_u16(&sTmp117TaskTimer, TMP117_TASK_PERIOD_MS))
        return;

    if (usTmp117TaskWaitTimer > 0)
    {
        usTmp117TaskWaitTimer--;
        return;
    }

    switch (eTmp117State)
    {
        case TMP117_STATE_IDLE:
            if (SysTimTestTimer1ms_u16(&sTmp117ReadTimer, TMP117_READ_PERIOD_MS))
            {
                eTmp117State = TMP117_STATE_REQ_TEMP;
            }
            break;

        case TMP117_STATE_REQ_TEMP:
            if (!TMP117_RequestRead(TMP117_REG_TEMP))
                break; // I2C busy, retry next tick
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_TEMP;
            break;

        case TMP117_STATE_WAIT_TX_TEMP:
            if (ucTmp117TxComplete)
            {
                ucTmp117TxComplete = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_REC_TEMP;
                usTmp117TaskWaitTimer = TMP117_WAIT_I2C_TICKS;
            }
            else if (ucTmp117Error)
            {
                ucTmp117Error = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            }
            else if (TMP117_CheckDmaTimeout())
            {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        case TMP117_STATE_REC_TEMP:
            if (!TMP117_RequestReadData())
                break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_RX_TEMP;
            break;

        case TMP117_STATE_WAIT_RX_TEMP:
            if (ucTmp117RxComplete)
            {
                ucTmp117RxComplete = 0;
                usTmp117DmaTimeout = 0;
                TMP117_ProcessTemp();
                eTmp117State = TMP117_STATE_REQ_ID;
            }
            else if (ucTmp117Error)
            {
                ucTmp117Error = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            }
            else if (TMP117_CheckDmaTimeout())
            {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        case TMP117_STATE_REQ_ID:
            if (!TMP117_RequestRead(TMP117_REG_ID))
                break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_ID;
            break;

        case TMP117_STATE_WAIT_TX_ID:
            if (ucTmp117TxComplete)
            {
                ucTmp117TxComplete = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_REC_ID;
                usTmp117TaskWaitTimer = TMP117_WAIT_I2C_TICKS;
            }
            else if (ucTmp117Error)
            {
                ucTmp117Error = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            }
            else if (TMP117_CheckDmaTimeout())
            {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        case TMP117_STATE_REC_ID:
            if (!TMP117_RequestReadData())
                break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_RX_ID;
            break;

        case TMP117_STATE_WAIT_RX_ID:
            if (ucTmp117RxComplete)
            {
                ucTmp117RxComplete = 0;
                usTmp117DmaTimeout = 0;
                TMP117_ProcessId();
                eTmp117State = TMP117_STATE_REQ_CONFIG;
            }
            else if (ucTmp117Error)
            {
                ucTmp117Error = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            }
            else if (TMP117_CheckDmaTimeout())
            {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        case TMP117_STATE_REQ_CONFIG:
            if (!TMP117_RequestRead(TMP117_REG_CONFIG))
                break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_CONFIG;
            break;

        case TMP117_STATE_WAIT_TX_CONFIG:
            if (ucTmp117TxComplete)
            {
                ucTmp117TxComplete = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_REC_CONFIG;
                usTmp117TaskWaitTimer = TMP117_WAIT_I2C_TICKS;
            }
            else if (ucTmp117Error)
            {
                ucTmp117Error = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            }
            else if (TMP117_CheckDmaTimeout())
            {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        case TMP117_STATE_REC_CONFIG:
            if (!TMP117_RequestReadData())
                break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_RX_CONFIG;
            break;

        case TMP117_STATE_WAIT_RX_CONFIG:
            if (ucTmp117RxComplete)
            {
                ucTmp117RxComplete = 0;
                usTmp117DmaTimeout = 0;
                TMP117_ProcessConfig();
                eTmp117State = TMP117_STATE_IDLE;
            }
            else if (ucTmp117Error)
            {
                ucTmp117Error = 0;
                usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            }
            else if (TMP117_CheckDmaTimeout())
            {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        case TMP117_STATE_ERROR:
            usTmp117TaskWaitTimer = TMP117_WAIT_ERROR_TICKS;
            eTmp117State = TMP117_STATE_REQ_TEMP;
            break;

        default:
            eTmp117State = TMP117_STATE_ERROR;
            break;
    }
}

void TMP117_I2C_TxComplete_Callback(void)
{
    g_i2c_active_sensor = I2C_SENSOR_NONE;
    ucTmp117TxComplete = 1;
}

void TMP117_I2C_RxComplete_Callback(void)
{
    g_i2c_active_sensor = I2C_SENSOR_NONE;
    ucTmp117RxComplete = 1;
}

void TMP117_I2C_Error_Callback(void)
{
    g_i2c_active_sensor = I2C_SENSOR_NONE;
    ucTmp117Error = 1;
    eTmp117State = TMP117_STATE_ERROR;
}
