/*
 * tmp117.h
 *
 *  Created on: 19 mar 2026
 *      Author: bniemiec
 */

#ifndef SENSOR_TMP117_H_
#define SENSOR_TMP117_H_

#include "main.h"

#define TMP117_ADDR (0x48 << 1)

HAL_StatusTypeDef TMP117_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef TMP117_Read(float *temp, uint16_t *id);

#endif /* SENSOR_TMP117_H_ */
