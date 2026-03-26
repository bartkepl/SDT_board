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

typedef enum{
	eSht45SensorState_Init = 0,
	eSht45SensorState_Wait,
	eSht45SensorState_RequestMeas,
	eSht45SensorState_ReceiveMeas,
	eSht45SensorState_RequestSN,
	eSht45SensorState_ReceiveSN,
	eSht45SensorState_RequestHeater,
	eSht45SensorState_ReceiveHeater,
	eSht45SensorState_RequestSoftReset,

	eeSht45SensorState_Error,
}Sht45SensorState_t;

typedef struct{
//	MeasPrecision_t eMeasPrec;
	uint16_t sTempData;
	uint16_t sHumData;

	float fTempData;
	float fHumData;

	uint32_t iSerialNumber;

	uint8_t cSoftResetFlag;

	uint8_t cHeaterActivationFlag;
//	HeaterMode_t eHeaterMode;

} SensorSht45Data_t;

HAL_StatusTypeDef SHT45_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef SHT45_Read(float *temp, float *hum, uint32_t *id);

HAL_StatusTypeDef SHT45_Heater(void);

#endif /* SENSOR_SHT45_H_ */
