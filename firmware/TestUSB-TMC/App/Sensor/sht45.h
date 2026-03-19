/*
 * sht45.h
 *
 *  Created on: 19 mar 2026
 *      Author: bniemiec
 */

#ifndef SENSOR_SHT45_H_
#define SENSOR_SHT45_H_

#include "main.h"

#define SHT45_ADDR (0x44 << 1)

HAL_StatusTypeDef SHT45_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef SHT45_Read(float *temp, float *hum, uint32_t *id);

#endif /* SENSOR_SHT45_H_ */
