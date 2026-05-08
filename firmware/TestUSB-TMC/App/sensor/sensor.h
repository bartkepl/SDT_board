/*
 * sensor.h
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#ifndef SENSOR_SENSOR_H_
#define SENSOR_SENSOR_H_

#include "main.h"
#include <stdbool.h>

typedef enum
{
    SENSOR_NONE = 0,
    SENSOR_TMP117,
    SENSOR_SHT45,
	SENSOR_DUAL,
	SENSOR_ERROR,
} SensorType_t;

// Unified sensor data structure
typedef struct
{
	// Universal data
    SensorType_t type;
    float fTemp;
    float fHum;
    uint32_t usSensorId;
    uint8_t ucValidFlag;
    uint8_t ucNewDataFlag;
    uint8_t ucInitializedFlag;
    
    // Timers
    uint16_t sMeasTaskTimer;
} SensorData_t;

extern SensorData_t g_sensor;

typedef enum {
    SENSOR_ERR_NONE = 0,
    SENSOR_ERR_NOT_FOUND,   /* czujnik nie wykryty podczas init -> SCPI -241 */
    SENSOR_ERR_COMM,        /* blad magistrali I2C -> SCPI -240 */
    SENSOR_ERR_TIMEOUT,     /* timeout DMA/I2C -> SCPI -365 */
    SENSOR_ERR_DATA,        /* blad CRC lub dane niewazne -> SCPI -230 */
} SensorError_t;

SensorError_t Sensor_GetAndClearError(void);

// I2C bus arbitration for DUAL sensor mode
#define I2C_SENSOR_NONE   0u
#define I2C_SENSOR_TMP117 1u
#define I2C_SENSOR_SHT45  2u

extern volatile uint8_t g_i2c_active_sensor;

// Initialization
void Sensor_Init(I2C_HandleTypeDef *hi2c);

// Main task (call every 1-10ms)
void Sensor_Task(void);

// I2C callback functions (to be called from HAL I2C complete/error callbacks)
void Sensor_I2C_TxComplete_Callback(void);
void Sensor_I2C_RxComplete_Callback(void);
void Sensor_I2C_Error_Callback(void);

// SHT45 specific functions
void Sensor_SHT45_RequestHeater(uint8_t ucHeaterMode);
void Sensor_SHT45_RequestSoftReset(void);
void Sensor_SHT45_SetPrecision(uint8_t ucPrecision);

// SHT45 configuration management (SCPI control)
void Sensor_SHT45_SetReadPeriod(uint16_t periodMs);
uint16_t Sensor_SHT45_GetReadPeriod(void);
void Sensor_SHT45_SetAverageCount(uint8_t count);
uint8_t Sensor_SHT45_GetAverageCount(void);
void Sensor_SHT45_SetMeasurementPrecision(uint8_t precision);
uint8_t Sensor_SHT45_GetMeasurementPrecision(void);

// TMP117 specific functions
void     Sensor_TMP117_SetMode(uint8_t mode);
uint8_t  Sensor_TMP117_GetMode(void);
void     Sensor_TMP117_SetConvRate(uint8_t rate);
uint8_t  Sensor_TMP117_GetConvRate(void);
void     Sensor_TMP117_SetAvgHW(uint8_t avg);
uint8_t  Sensor_TMP117_GetAvgHW(void);
void     Sensor_TMP117_SetReadPeriod(uint16_t periodMs);
uint16_t Sensor_TMP117_GetReadPeriod(void);
void     Sensor_TMP117_SetAlertHigh(float fTemp);
float    Sensor_TMP117_GetAlertHigh(void);
void     Sensor_TMP117_SetAlertLow(float fTemp);
float    Sensor_TMP117_GetAlertLow(void);
uint8_t  Sensor_TMP117_GetAlertStatus(void);
void     Sensor_TMP117_RequestSoftReset(void);

#endif /* SENSOR_SENSOR_H_ */
