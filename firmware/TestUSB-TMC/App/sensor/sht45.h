/*
 * sht45.h
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#ifndef SENSOR_SHT45_H_
#define SENSOR_SHT45_H_

#include "main.h"

#define SHT45_ADDR (0x44 << 1)

// SHT45 Commands
#define SHT45_CMD_MEASURE_HIGH_PRECISION    0xFD
#define SHT45_CMD_MEASURE_MED_PRECISION     0xF6
#define SHT45_CMD_MEASURE_LOW_PRECISION     0xE0
#define SHT45_CMD_READ_SERIAL_NUMB          0x89
#define SHT45_CMD_SOFT_RESET                0x94
#define SHT45_CMD_SOFT_RESET_RESTART        0xB0
#define SHT45_CMD_HEATER_200MW_1S           0x39
#define SHT45_CMD_HEATER_200MW_100MS        0x32
#define SHT45_CMD_HEATER_110MW_1S           0x2F
#define SHT45_CMD_HEATER_110MW_100MS        0x24
#define SHT45_CMD_HEATER_20MW_1S            0x1E
#define SHT45_CMD_HEATER_20MW_100MS         0x15

// SHT45 State Machine
typedef enum {
    SHT45_STATE_IDLE = 0,
    SHT45_STATE_REQ_MEASURE,
    SHT45_STATE_REC_MEASURE,
	SHT45_STATE_PROC_MEASURE,
    SHT45_STATE_REQ_SERIAL,
    SHT45_STATE_REC_SERIAL,
	SHT45_STATE_PROC_SERIAL,
    SHT45_STATE_REQ_CONFIG,
    SHT45_STATE_REC_CONFIG,
    SHT45_STATE_REQ_HEATER,
    SHT45_STATE_REC_HEATER,
    SHT45_STATE_REQ_SOFTRESET,
    SHT45_STATE_WAIT_SOFTRESET,
    SHT45_STATE_REQ_SOFTRESET_RESTART,
    SHT45_STATE_WAIT_SOFTRESET_COMPLETE,
    SHT45_STATE_ERROR,
} SHT45_State_t;

// SHT45 Measurement Precision
typedef enum {
    SHT45_PRECISION_HIGH = 0xFD,
    SHT45_PRECISION_MEDIUM = 0xF6,
    SHT45_PRECISION_LOW = 0xE0,
} SHT45_Precision_t;

// SHT45 Heater Mode
typedef enum {
    SHT45_HEATER_200MW_1S = 0x39,
    SHT45_HEATER_200MW_100MS = 0x32,
    SHT45_HEATER_110MW_1S = 0x2F,
    SHT45_HEATER_110MW_100MS = 0x24,
    SHT45_HEATER_20MW_1S = 0x1E,
    SHT45_HEATER_20MW_100MS = 0x15
} SHT45_HeaterMode_t;

// SHT45 Data structure
typedef struct {
    float fTemp;
    float fHum;
    uint32_t uSerialNumber;
    uint8_t ucValidFlag;
    uint8_t ucNewDataFlag;
    uint8_t ucInitializedFlag;
    
    // Control flags
    uint8_t ucHeaterActivationFlag;
    uint8_t ucSoftResetFlag;
    SHT45_Precision_t eMeasPrecision;
    SHT45_HeaterMode_t eHeaterMode;
} SHT45_Data_t;

extern SHT45_Data_t g_sht45;

void SHT45_Init(I2C_HandleTypeDef *hi2c);
void SHT45_Task(void);
void SHT45_I2C_Complete_Callback(void);
void SHT45_I2C_Error_Callback(void);

// Public command functions
void SHT45_RequestHeater(SHT45_HeaterMode_t eMode);
void SHT45_RequestSoftReset(void);
void SHT45_SetMeasurementPrecision(SHT45_Precision_t ePrecision);

#endif /* SENSOR_SHT45_H_ */
