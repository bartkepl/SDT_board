/*
 * temp_sensor.h
 *
 *  Created on: 16 mar 2026
 *      Author: bartkepl
 */

#ifndef INC_TEMP_SENSOR_H_
#define INC_TEMP_SENSOR_H_

#include "main.h"
#include <stdint.h>

typedef enum
{
    TEMP_NONE = 0,
    TEMP_TMP117,
    TEMP_SHT45
} TempSensorType;

typedef struct
{
    TempSensorType type;
    uint8_t address;
} TempSensor;

#define TEMP_SENSOR_MAX 2

extern TempSensor tempSensors[TEMP_SENSOR_MAX];
extern uint8_t tempSensorCount;

HAL_StatusTypeDef TempSensors_Init(I2C_HandleTypeDef *hi2c);

float TempSensor_Read(uint8_t id);
void Display_ShowTemperature(uint8_t sensorID);

#endif /* INC_TEMP_SENSOR_H_ */
