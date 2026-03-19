/*
 * sensor.h
 *
 *  Created on: 19 mar 2026
 *      Author: bniemiec
 */

#ifndef SENSOR_SENSOR_H_
#define SENSOR_SENSOR_H_

#include "main.h"
#include <stdbool.h>

typedef enum
{
    SENSOR_NONE = 0,
    SENSOR_TMP117,
    SENSOR_SHT45
} SensorType_t;

typedef struct
{
    SensorType_t type;
    uint16_t id;

    float temperature;
    float humidity;

    bool valid;
} SensorData_t;

extern SensorData_t g_sensor;

void Sensor_Init(I2C_HandleTypeDef *hi2c);
void Sensor_Task(void);

#endif /* SENSOR_SENSOR_H_ */
