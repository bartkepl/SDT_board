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

//SHT45
typedef enum{
	ePrecMeas_High = 0xFD,
	ePrecMeas_Medium = 0xF6,
	ePrecMeas_Low = 0xE0,
}MeasPrecision_t;

typedef enum{
	eHeaterMode_200mW_1s = 0x39,
	eHeaterMode_200mW_100ms = 0x32,
	eHeaterMode_110mW_1s = 0x2F,
	eHeaterMode_110mW_100ms = 0x24,
	eHeaterMode_20mW_1s = 0x1E,
	eHeaterMode_20mW_100ms = 0x15
}HeaterMode_t;


//TMP117

typedef struct
{
	//Universal data
    SensorType_t type;
    uint16_t usSensorId;
    uint16_t usTempRaw;
    float fTemp;
    uint8_t ucValidFlag;
    uint8_t ucNewDataFlag;
    uint8_t ucInitializedFlag;

    //SHT45 specyfic
    uint16_t usHumRaw;
    float fHum;
    MeasPrecision_t eSHT45MeasType;
    HeaterMode_t eSHT45HeaterMode;

    //TMP117 specyfic
    uint16_t tusTMP117Eeprom[3];
    uint16_t usTMP117ConfigRegister;

    // timery
    uint16_t sMeasTaskTimer;
    uint16_t sHeaterCooldownTime;
} SensorData_t;

extern SensorData_t g_sensor;

void Sensor_Init(I2C_HandleTypeDef *hi2c);
void Sensor_Task(void);

void Sensor_SHT45Heater(void);

#endif /* SENSOR_SENSOR_H_ */
