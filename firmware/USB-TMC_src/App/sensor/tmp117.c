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
static uint8_t  ucTmp117RxBuffer[2];
static uint8_t  ucTmp117TxBuffer[3];  // 3 bytes: [reg, dataHi, dataLo]
static uint16_t usTmp117TaskWaitTimer = 0;
static uint16_t usTmp117DmaTimeout = 0;
static volatile uint8_t ucTmp117TxComplete = 0;
static volatile uint8_t ucTmp117RxComplete = 0;
static volatile uint8_t ucTmp117Error      = 0;

static volatile uint8_t ucTmp117ConfigDirty = 0;
static volatile uint8_t ucTmp117ThighDirty  = 0;
static volatile uint8_t ucTmp117TlowDirty   = 0;

#define TMP117_TASK_PERIOD_MS       10
#define TMP117_READ_PERIOD_MS       1000
#define TMP117_DMA_TIMEOUT_TICKS    (100 / TMP117_TASK_PERIOD_MS)   // 10 ticks = 100ms
#define TMP117_WAIT_I2C_TICKS       (20  / TMP117_TASK_PERIOD_MS)   // 2  ticks = 20ms
#define TMP117_WAIT_ERROR_TICKS     (1000 / TMP117_TASK_PERIOD_MS)  // 100 ticks = 1000ms

TMP117_Data_t g_tmp117 = {0};

TMP117_Config_t g_tmp117_config = {
    .eMode          = TMP117_MODE_CONTINUOUS,
    .ucConvRate     = 4,                    // 1s (CONV=100)
    .eAvg           = TMP117_AVG_8,
    .usReadPeriodMs = TMP117_READ_PERIOD_MS,
    .sAlertHighRaw  = (int16_t)0x6000,      // +192°C hardware default
    .sAlertLowRaw   = (int16_t)0x8000,      // -256°C hardware default
};

//------------------------------------------------------------------//
// Private helpers
//------------------------------------------------------------------//

static int16_t TMP117_TempToRaw(float fTemp)
{
    return (int16_t)(fTemp / TMP117_TEMP_LSB);
}

static float TMP117_RawToTemp(int16_t sRaw)
{
    return sRaw * TMP117_TEMP_LSB;
}

// Build CONFIG register value from config struct, preserving read-only status bits.
// If SOFT_RESET is set in shadow, pass it through and clear it (one-shot).
static uint16_t TMP117_BuildConfigReg(void)
{
    uint16_t cfg = g_tmp117.usConfigRegister;

    // Clear mutable fields
    cfg &= ~(TMP117_CFG_MOD_MASK | TMP117_CFG_CONV_MASK | TMP117_CFG_AVG_MASK);

    // Apply config struct values
    cfg |= ((uint16_t)(g_tmp117_config.eMode     & 0x3u) << TMP117_CFG_MOD_SHIFT);
    cfg |= ((uint16_t)(g_tmp117_config.ucConvRate & 0x7u) << TMP117_CFG_CONV_SHIFT);
    cfg |= ((uint16_t)(g_tmp117_config.eAvg       & 0x3u) << TMP117_CFG_AVG_SHIFT);

    // Pass through SOFT_RESET if requested, then clear it from shadow
    if (g_tmp117.usConfigRegister & TMP117_CFG_SOFT_RESET)
    {
        cfg |= TMP117_CFG_SOFT_RESET;
        g_tmp117.usConfigRegister &= ~TMP117_CFG_SOFT_RESET;
    }

    return cfg;
}

// Returns false if I2C bus is busy by the other sensor (DUAL mode)
static bool TMP117_RequestRead(uint8_t ucRegister)
{
    if (g_i2c_active_sensor != I2C_SENSOR_NONE && g_i2c_active_sensor != I2C_SENSOR_TMP117)
        return false;

    g_i2c_active_sensor = I2C_SENSOR_TMP117;
    ucTmp117TxBuffer[0] = ucRegister;
    ucTmp117TxComplete  = 0;
    ucTmp117RxComplete  = 0;
    ucTmp117Error       = 0;

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
    ucTmp117TxComplete  = 0;
    ucTmp117RxComplete  = 0;
    ucTmp117Error       = 0;

    if (HAL_I2C_Master_Receive_DMA(i2c, TMP117_ADDR, ucTmp117RxBuffer, 2) != HAL_OK)
    {
        g_i2c_active_sensor = I2C_SENSOR_NONE;
        eTmp117State = TMP117_STATE_ERROR;
        return false;
    }
    return true;
}

// Write a 16-bit register value (3-byte DMA Tx: reg + dataHi + dataLo)
static bool TMP117_RequestWrite(uint8_t ucRegister, uint16_t usValue)
{
    if (g_i2c_active_sensor != I2C_SENSOR_NONE && g_i2c_active_sensor != I2C_SENSOR_TMP117)
        return false;

    g_i2c_active_sensor = I2C_SENSOR_TMP117;
    ucTmp117TxBuffer[0] = ucRegister;
    ucTmp117TxBuffer[1] = (uint8_t)(usValue >> 8);
    ucTmp117TxBuffer[2] = (uint8_t)(usValue & 0xFF);
    ucTmp117TxComplete  = 0;
    ucTmp117Error       = 0;

    if (HAL_I2C_Master_Transmit_DMA(i2c, TMP117_ADDR, ucTmp117TxBuffer, 3) != HAL_OK)
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
    g_tmp117.fTemp = raw * TMP117_TEMP_LSB;
    g_tmp117.ucValidFlag    = 1;
    g_tmp117.ucNewDataFlag  = 1;
}

static void TMP117_ProcessId(void)
{
    g_tmp117.usId = ((uint16_t)ucTmp117RxBuffer[0] << 8) | ucTmp117RxBuffer[1];
}

static void TMP117_ProcessConfig(void)
{
    g_tmp117.usConfigRegister = ((uint16_t)ucTmp117RxBuffer[0] << 8) | ucTmp117RxBuffer[1];

    // After a soft reset the hardware reverts to 0x0220. If the shadow no longer
    // matches what we want, re-push the config automatically.
    if ((g_tmp117.usConfigRegister & ~(TMP117_CFG_HIGH_ALERT | TMP117_CFG_LOW_ALERT |
                                       TMP117_CFG_DATA_READY  | TMP117_CFG_EEPROM_BUSY))
        != (TMP117_BuildConfigReg() & ~(TMP117_CFG_HIGH_ALERT | TMP117_CFG_LOW_ALERT |
                                        TMP117_CFG_DATA_READY  | TMP117_CFG_EEPROM_BUSY)))
    {
        ucTmp117ConfigDirty = 1;
    }
}

static bool TMP117_CheckDmaTimeout(void)
{
    if (++usTmp117DmaTimeout >= TMP117_DMA_TIMEOUT_TICKS)
    {
        usTmp117DmaTimeout  = 0;
        g_i2c_active_sensor = I2C_SENSOR_NONE;
        return true;
    }
    return false;
}

//------------------------------------------------------------------//
// Macros to reduce repetition in WAIT_TX/WAIT_RX states
//------------------------------------------------------------------//

// Use in every WAIT_TX state: on complete clear flags and advance to next_state,
// on error or timeout go to ERROR.
#define TMP117_HANDLE_WAIT_TX(next_state)                          \
    if (ucTmp117TxComplete) {                                      \
        ucTmp117TxComplete = 0; usTmp117DmaTimeout = 0;           \
        eTmp117State = (next_state);                               \
    } else if (ucTmp117Error) {                                    \
        ucTmp117Error = 0; usTmp117DmaTimeout = 0;                \
        eTmp117State = TMP117_STATE_ERROR;                         \
    } else if (TMP117_CheckDmaTimeout()) {                         \
        eTmp117State = TMP117_STATE_ERROR;                         \
    }

#define TMP117_HANDLE_WAIT_RX(next_state)                          \
    if (ucTmp117RxComplete) {                                      \
        ucTmp117RxComplete = 0; usTmp117DmaTimeout = 0;           \
        eTmp117State = (next_state);                               \
    } else if (ucTmp117Error) {                                    \
        ucTmp117Error = 0; usTmp117DmaTimeout = 0;                \
        eTmp117State = TMP117_STATE_ERROR;                         \
    } else if (TMP117_CheckDmaTimeout()) {                         \
        eTmp117State = TMP117_STATE_ERROR;                         \
    }

//------------------------------------------------------------------//
// Public functions
//------------------------------------------------------------------//

void TMP117_Init(I2C_HandleTypeDef *hi2c)
{
    i2c = hi2c;

    g_tmp117.ucValidFlag       = 0;
    g_tmp117.ucNewDataFlag     = 0;
    g_tmp117.ucInitializedFlag = 1;
    g_tmp117.ucInitReadDone    = 0;
    g_tmp117.fTemp             = 0;
    g_tmp117.usId              = 0;
    g_tmp117.usConfigRegister  = TMP117_CFG_DEFAULT;
    g_tmp117.sAlertHighRaw     = (int16_t)0x6000;
    g_tmp117.sAlertLowRaw      = (int16_t)0x8000;

    ucTmp117TxComplete  = 0;
    ucTmp117RxComplete  = 0;
    ucTmp117Error       = 0;
    ucTmp117ConfigDirty = 0;
    ucTmp117ThighDirty  = 0;
    ucTmp117TlowDirty   = 0;

    eTmp117State          = TMP117_STATE_IDLE;
    usTmp117TaskWaitTimer = 0;
    usTmp117DmaTimeout    = 0;
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
        // ---- IDLE: check pending writes first, then normal read cycle ----
        case TMP117_STATE_IDLE:
            if (ucTmp117ConfigDirty) {
                ucTmp117ConfigDirty = 0;
                eTmp117State = TMP117_STATE_REQ_WRITE_CONFIG;
                break;
            }
            if (ucTmp117ThighDirty) {
                ucTmp117ThighDirty = 0;
                eTmp117State = TMP117_STATE_REQ_WRITE_THIGH;
                break;
            }
            if (ucTmp117TlowDirty) {
                ucTmp117TlowDirty = 0;
                eTmp117State = TMP117_STATE_REQ_WRITE_TLOW;
                break;
            }
            if (SysTimTestTimer1ms_u16(&sTmp117ReadTimer, g_tmp117_config.usReadPeriodMs))
                eTmp117State = TMP117_STATE_REQ_TEMP;
            break;

        // ---- Normal read cycle: TEMP → ID → CONFIG ----
        case TMP117_STATE_REQ_TEMP:
            if (!TMP117_RequestRead(TMP117_REG_TEMP)) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_TEMP;
            break;

        case TMP117_STATE_WAIT_TX_TEMP:
            TMP117_HANDLE_WAIT_TX(TMP117_STATE_REC_TEMP);
            if (eTmp117State == TMP117_STATE_REC_TEMP)
                usTmp117TaskWaitTimer = TMP117_WAIT_I2C_TICKS;
            break;

        case TMP117_STATE_REC_TEMP:
            if (!TMP117_RequestReadData()) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_RX_TEMP;
            break;

        case TMP117_STATE_WAIT_RX_TEMP:
            if (ucTmp117RxComplete) {
                ucTmp117RxComplete = 0; usTmp117DmaTimeout = 0;
                TMP117_ProcessTemp();
                eTmp117State = TMP117_STATE_REQ_ID;
            } else if (ucTmp117Error) {
                ucTmp117Error = 0; usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            } else if (TMP117_CheckDmaTimeout()) {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        case TMP117_STATE_REQ_ID:
            if (!TMP117_RequestRead(TMP117_REG_ID)) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_ID;
            break;

        case TMP117_STATE_WAIT_TX_ID:
            TMP117_HANDLE_WAIT_TX(TMP117_STATE_REC_ID);
            if (eTmp117State == TMP117_STATE_REC_ID)
                usTmp117TaskWaitTimer = TMP117_WAIT_I2C_TICKS;
            break;

        case TMP117_STATE_REC_ID:
            if (!TMP117_RequestReadData()) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_RX_ID;
            break;

        case TMP117_STATE_WAIT_RX_ID:
            if (ucTmp117RxComplete) {
                ucTmp117RxComplete = 0; usTmp117DmaTimeout = 0;
                TMP117_ProcessId();
                eTmp117State = TMP117_STATE_REQ_CONFIG;
            } else if (ucTmp117Error) {
                ucTmp117Error = 0; usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            } else if (TMP117_CheckDmaTimeout()) {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        case TMP117_STATE_REQ_CONFIG:
            if (!TMP117_RequestRead(TMP117_REG_CONFIG)) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_CONFIG;
            break;

        case TMP117_STATE_WAIT_TX_CONFIG:
            TMP117_HANDLE_WAIT_TX(TMP117_STATE_REC_CONFIG);
            if (eTmp117State == TMP117_STATE_REC_CONFIG)
                usTmp117TaskWaitTimer = TMP117_WAIT_I2C_TICKS;
            break;

        case TMP117_STATE_REC_CONFIG:
            if (!TMP117_RequestReadData()) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_RX_CONFIG;
            break;

        case TMP117_STATE_WAIT_RX_CONFIG:
            if (ucTmp117RxComplete) {
                ucTmp117RxComplete = 0; usTmp117DmaTimeout = 0;
                TMP117_ProcessConfig();
                // Read THIGH/TLOW once after first CONFIG read
                eTmp117State = g_tmp117.ucInitReadDone ? TMP117_STATE_IDLE
                                                       : TMP117_STATE_REQ_THIGH;
            } else if (ucTmp117Error) {
                ucTmp117Error = 0; usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            } else if (TMP117_CheckDmaTimeout()) {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        // ---- Write CONFIG ----
        case TMP117_STATE_REQ_WRITE_CONFIG:
            if (!TMP117_RequestWrite(TMP117_REG_CONFIG, TMP117_BuildConfigReg())) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_WRITE_CONFIG;
            break;

        case TMP117_STATE_WAIT_TX_WRITE_CONFIG:
            if (ucTmp117TxComplete) {
                ucTmp117TxComplete = 0; usTmp117DmaTimeout = 0;
                g_tmp117.usConfigRegister = TMP117_BuildConfigReg();
                eTmp117State = TMP117_STATE_IDLE;
            } else if (ucTmp117Error) {
                ucTmp117Error = 0; usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            } else if (TMP117_CheckDmaTimeout()) {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        // ---- Write THIGH ----
        case TMP117_STATE_REQ_WRITE_THIGH:
            if (!TMP117_RequestWrite(TMP117_REG_THIGH, (uint16_t)g_tmp117_config.sAlertHighRaw)) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_WRITE_THIGH;
            break;

        case TMP117_STATE_WAIT_TX_WRITE_THIGH:
            if (ucTmp117TxComplete) {
                ucTmp117TxComplete = 0; usTmp117DmaTimeout = 0;
                g_tmp117.sAlertHighRaw = g_tmp117_config.sAlertHighRaw;
                eTmp117State = TMP117_STATE_IDLE;
            } else if (ucTmp117Error) {
                ucTmp117Error = 0; usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            } else if (TMP117_CheckDmaTimeout()) {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        // ---- Write TLOW ----
        case TMP117_STATE_REQ_WRITE_TLOW:
            if (!TMP117_RequestWrite(TMP117_REG_TLOW, (uint16_t)g_tmp117_config.sAlertLowRaw)) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_WRITE_TLOW;
            break;

        case TMP117_STATE_WAIT_TX_WRITE_TLOW:
            if (ucTmp117TxComplete) {
                ucTmp117TxComplete = 0; usTmp117DmaTimeout = 0;
                g_tmp117.sAlertLowRaw = g_tmp117_config.sAlertLowRaw;
                eTmp117State = TMP117_STATE_IDLE;
            } else if (ucTmp117Error) {
                ucTmp117Error = 0; usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            } else if (TMP117_CheckDmaTimeout()) {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        // ---- Read THIGH (init read sequence) ----
        case TMP117_STATE_REQ_THIGH:
            if (!TMP117_RequestRead(TMP117_REG_THIGH)) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_THIGH;
            break;

        case TMP117_STATE_WAIT_TX_THIGH:
            TMP117_HANDLE_WAIT_TX(TMP117_STATE_REC_THIGH);
            if (eTmp117State == TMP117_STATE_REC_THIGH)
                usTmp117TaskWaitTimer = TMP117_WAIT_I2C_TICKS;
            break;

        case TMP117_STATE_REC_THIGH:
            if (!TMP117_RequestReadData()) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_RX_THIGH;
            break;

        case TMP117_STATE_WAIT_RX_THIGH:
            if (ucTmp117RxComplete) {
                ucTmp117RxComplete = 0; usTmp117DmaTimeout = 0;
                g_tmp117.sAlertHighRaw = (int16_t)(((uint16_t)ucTmp117RxBuffer[0] << 8)
                                                   | ucTmp117RxBuffer[1]);
                g_tmp117_config.sAlertHighRaw = g_tmp117.sAlertHighRaw;
                eTmp117State = TMP117_STATE_REQ_TLOW;
            } else if (ucTmp117Error) {
                ucTmp117Error = 0; usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            } else if (TMP117_CheckDmaTimeout()) {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        // ---- Read TLOW ----
        case TMP117_STATE_REQ_TLOW:
            if (!TMP117_RequestRead(TMP117_REG_TLOW)) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_TX_TLOW;
            break;

        case TMP117_STATE_WAIT_TX_TLOW:
            TMP117_HANDLE_WAIT_TX(TMP117_STATE_REC_TLOW);
            if (eTmp117State == TMP117_STATE_REC_TLOW)
                usTmp117TaskWaitTimer = TMP117_WAIT_I2C_TICKS;
            break;

        case TMP117_STATE_REC_TLOW:
            if (!TMP117_RequestReadData()) break;
            usTmp117DmaTimeout = 0;
            eTmp117State = TMP117_STATE_WAIT_RX_TLOW;
            break;

        case TMP117_STATE_WAIT_RX_TLOW:
            if (ucTmp117RxComplete) {
                ucTmp117RxComplete = 0; usTmp117DmaTimeout = 0;
                g_tmp117.sAlertLowRaw = (int16_t)(((uint16_t)ucTmp117RxBuffer[0] << 8)
                                                  | ucTmp117RxBuffer[1]);
                g_tmp117_config.sAlertLowRaw = g_tmp117.sAlertLowRaw;
                g_tmp117.ucInitReadDone = 1;
                eTmp117State = TMP117_STATE_IDLE;
            } else if (ucTmp117Error) {
                ucTmp117Error = 0; usTmp117DmaTimeout = 0;
                eTmp117State = TMP117_STATE_ERROR;
            } else if (TMP117_CheckDmaTimeout()) {
                eTmp117State = TMP117_STATE_ERROR;
            }
            break;

        // ---- Error ----
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
    ucTmp117TxComplete  = 1;
}

void TMP117_I2C_RxComplete_Callback(void)
{
    g_i2c_active_sensor = I2C_SENSOR_NONE;
    ucTmp117RxComplete  = 1;
}

void TMP117_I2C_Error_Callback(void)
{
    g_i2c_active_sensor = I2C_SENSOR_NONE;
    ucTmp117Error       = 1;
    eTmp117State        = TMP117_STATE_ERROR;
}

//------------------------------------------------------------------//
// Public configuration API
//------------------------------------------------------------------//

void TMP117_SetMode(TMP117_Mode_t eMode)
{
    g_tmp117_config.eMode = eMode;
    ucTmp117ConfigDirty   = 1;
}

TMP117_Mode_t TMP117_GetMode(void)
{
    return g_tmp117_config.eMode;
}

void TMP117_SetConvRate(uint8_t ucRate)
{
    g_tmp117_config.ucConvRate = (ucRate > 7u) ? 7u : ucRate;
    ucTmp117ConfigDirty        = 1;
}

uint8_t TMP117_GetConvRate(void)
{
    return g_tmp117_config.ucConvRate;
}

void TMP117_SetAvgHW(TMP117_Averaging_t eAvg)
{
    g_tmp117_config.eAvg = eAvg;
    ucTmp117ConfigDirty  = 1;
}

TMP117_Averaging_t TMP117_GetAvgHW(void)
{
    return g_tmp117_config.eAvg;
}

void TMP117_SetReadPeriod(uint16_t periodMs)
{
    g_tmp117_config.usReadPeriodMs = periodMs;
    SysTimZeroTimer1ms_u16(&sTmp117ReadTimer);
}

uint16_t TMP117_GetReadPeriod(void)
{
    return g_tmp117_config.usReadPeriodMs;
}

void TMP117_SetAlertHigh(float fTemp)
{
    g_tmp117_config.sAlertHighRaw = TMP117_TempToRaw(fTemp);
    ucTmp117ThighDirty            = 1;
}

float TMP117_GetAlertHigh(void)
{
    return TMP117_RawToTemp(g_tmp117.sAlertHighRaw);
}

void TMP117_SetAlertLow(float fTemp)
{
    g_tmp117_config.sAlertLowRaw = TMP117_TempToRaw(fTemp);
    ucTmp117TlowDirty            = 1;
}

float TMP117_GetAlertLow(void)
{
    return TMP117_RawToTemp(g_tmp117.sAlertLowRaw);
}

uint8_t TMP117_GetAlertStatus(void)
{
    uint8_t status = 0;
    if (g_tmp117.usConfigRegister & TMP117_CFG_HIGH_ALERT)  status |= 0x02u;
    if (g_tmp117.usConfigRegister & TMP117_CFG_LOW_ALERT)   status |= 0x01u;
    if (g_tmp117.usConfigRegister & TMP117_CFG_DATA_READY)  status |= 0x04u;
    return status;
}

void TMP117_RequestSoftReset(void)
{
    // Set SOFT_RESET bit in shadow; BuildConfigReg will pass it through once.
    g_tmp117.usConfigRegister |= TMP117_CFG_SOFT_RESET;
    ucTmp117ConfigDirty        = 1;
}
