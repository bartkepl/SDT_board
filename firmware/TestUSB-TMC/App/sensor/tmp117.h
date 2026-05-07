/*
 * tmp117.h
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#ifndef SENSOR_TMP117_H_
#define SENSOR_TMP117_H_

#include "main.h"

#define TMP117_ADDR (0x48 << 1)

// TMP117 Registers
#define TMP117_REG_TEMP         0x00
#define TMP117_REG_CONFIG       0x01
#define TMP117_REG_THIGH        0x02
#define TMP117_REG_TLOW         0x03
#define TMP117_REG_ID           0x0F

// CONFIG register bit masks
#define TMP117_CFG_HIGH_ALERT   (1u << 15)    // R - high alert flag
#define TMP117_CFG_LOW_ALERT    (1u << 14)    // R - low alert flag
#define TMP117_CFG_DATA_READY   (1u << 13)    // R - data ready flag
#define TMP117_CFG_EEPROM_BUSY  (1u << 12)    // R - EEPROM busy
#define TMP117_CFG_MOD_MASK     (0x3u << 10)  // conversion mode
#define TMP117_CFG_MOD_SHIFT    10u
#define TMP117_CFG_CONV_MASK    (0x7u << 7)   // conversion cycle
#define TMP117_CFG_CONV_SHIFT   7u
#define TMP117_CFG_AVG_MASK     (0x3u << 5)   // hardware averaging
#define TMP117_CFG_AVG_SHIFT    5u
#define TMP117_CFG_SOFT_RESET   (1u << 1)

// Power-on default: MOD=00 continuous, CONV=100 (1s), AVG=01 (8x)
#define TMP117_CFG_DEFAULT      0x0220u

// 1 LSB = 7.8125 m°C
#define TMP117_TEMP_LSB         0.0078125f

// Conversion cycle codes (CONV[2:0]):
// 0=15.5ms 1=125ms 2=250ms 3=500ms 4=1s 5=4s 6=8s 7=16s

// TMP117 conversion mode
typedef enum {
    TMP117_MODE_CONTINUOUS = 0x00,
    TMP117_MODE_SHUTDOWN   = 0x01,
    TMP117_MODE_ONESHOT    = 0x03,
} TMP117_Mode_t;

// Hardware averaging
typedef enum {
    TMP117_AVG_1  = 0x00,  // no averaging
    TMP117_AVG_8  = 0x01,  // 8x  (default)
    TMP117_AVG_32 = 0x02,  // 32x
    TMP117_AVG_64 = 0x03,  // 64x
} TMP117_Averaging_t;

// TMP117 State Machine
typedef enum {
    TMP117_STATE_IDLE = 0,
    TMP117_STATE_REQ_TEMP,
    TMP117_STATE_WAIT_TX_TEMP,
    TMP117_STATE_REC_TEMP,
    TMP117_STATE_WAIT_RX_TEMP,
    TMP117_STATE_REQ_ID,
    TMP117_STATE_WAIT_TX_ID,
    TMP117_STATE_REC_ID,
    TMP117_STATE_WAIT_RX_ID,
    TMP117_STATE_REQ_CONFIG,
    TMP117_STATE_WAIT_TX_CONFIG,
    TMP117_STATE_REC_CONFIG,
    TMP117_STATE_WAIT_RX_CONFIG,
    // Write states (TX only, no RX)
    TMP117_STATE_REQ_WRITE_CONFIG,
    TMP117_STATE_WAIT_TX_WRITE_CONFIG,
    TMP117_STATE_REQ_WRITE_THIGH,
    TMP117_STATE_WAIT_TX_WRITE_THIGH,
    TMP117_STATE_REQ_WRITE_TLOW,
    TMP117_STATE_WAIT_TX_WRITE_TLOW,
    // Read THIGH/TLOW (init once + on-demand)
    TMP117_STATE_REQ_THIGH,
    TMP117_STATE_WAIT_TX_THIGH,
    TMP117_STATE_REC_THIGH,
    TMP117_STATE_WAIT_RX_THIGH,
    TMP117_STATE_REQ_TLOW,
    TMP117_STATE_WAIT_TX_TLOW,
    TMP117_STATE_REC_TLOW,
    TMP117_STATE_WAIT_RX_TLOW,
    TMP117_STATE_ERROR,
} TMP117_State_t;

// Runtime configuration (writable fields)
typedef struct {
    TMP117_Mode_t       eMode;
    uint8_t             ucConvRate;     // CONV[2:0] raw value 0-7
    TMP117_Averaging_t  eAvg;
    uint16_t            usReadPeriodMs; // firmware polling period
    int16_t             sAlertHighRaw;  // THIGH shadow (raw int16)
    int16_t             sAlertLowRaw;   // TLOW shadow (raw int16)
} TMP117_Config_t;

// Data structure
typedef struct {
    float           fTemp;
    uint16_t        usId;
    uint16_t        usConfigRegister;  // CONFIG shadow
    int16_t         sAlertHighRaw;     // THIGH shadow
    int16_t         sAlertLowRaw;      // TLOW shadow
    uint8_t         ucValidFlag;
    uint8_t         ucNewDataFlag;
    uint8_t         ucInitializedFlag;
    uint8_t         ucInitReadDone;    // 1 after first THIGH/TLOW read
} TMP117_Data_t;

extern TMP117_Data_t   g_tmp117;
extern TMP117_Config_t g_tmp117_config;

// Core driver functions
void TMP117_Init(I2C_HandleTypeDef *hi2c);
void TMP117_Task(void);
void TMP117_I2C_TxComplete_Callback(void);
void TMP117_I2C_RxComplete_Callback(void);
void TMP117_I2C_Error_Callback(void);

// Mode / conversion rate / averaging
void               TMP117_SetMode(TMP117_Mode_t eMode);
TMP117_Mode_t      TMP117_GetMode(void);
void               TMP117_SetConvRate(uint8_t ucRate);   // 0-7
uint8_t            TMP117_GetConvRate(void);
void               TMP117_SetAvgHW(TMP117_Averaging_t eAvg);
TMP117_Averaging_t TMP117_GetAvgHW(void);

// Firmware read period
void               TMP117_SetReadPeriod(uint16_t periodMs);
uint16_t           TMP117_GetReadPeriod(void);

// Alert thresholds (°C)
void               TMP117_SetAlertHigh(float fTemp);
float              TMP117_GetAlertHigh(void);
void               TMP117_SetAlertLow(float fTemp);
float              TMP117_GetAlertLow(void);

// Alert status: bit1=HIGH active, bit0=LOW active, bit2=DATA_READY
uint8_t            TMP117_GetAlertStatus(void);

// Soft reset
void               TMP117_RequestSoftReset(void);

#endif /* SENSOR_TMP117_H_ */
