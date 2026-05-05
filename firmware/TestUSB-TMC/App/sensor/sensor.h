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
void Sensor_I2C_Complete_Callback(void);
void Sensor_I2C_Error_Callback(void);

// SHT45 specific functions
void Sensor_SHT45_RequestHeater(uint8_t ucHeaterMode);
void Sensor_SHT45_RequestSoftReset(void);
void Sensor_SHT45_SetPrecision(uint8_t ucPrecision);

#endif /* SENSOR_SENSOR_H_ */
