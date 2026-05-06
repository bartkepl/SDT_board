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

#endif /* SENSOR_SENSOR_H_ */
