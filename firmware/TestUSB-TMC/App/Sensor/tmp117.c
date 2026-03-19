/*
 * tmp117.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */


#include "tmp117.h"

static I2C_HandleTypeDef *i2c;

#define TMP117_REG_TEMP 0x00
#define TMP117_REG_ID   0x0F

HAL_StatusTypeDef TMP117_Init(I2C_HandleTypeDef *hi2c)
{
    i2c = hi2c;
    return HAL_OK;
}

HAL_StatusTypeDef TMP117_Read(float *temp, uint16_t *id)
{
    uint8_t buf[2];

    // temperature
    if (HAL_I2C_Mem_Read(i2c, TMP117_ADDR, TMP117_REG_TEMP,
                         I2C_MEMADD_SIZE_8BIT, buf, 2, 100) != HAL_OK)
        return HAL_ERROR;

    int16_t raw = (buf[0] << 8) | buf[1];
    *temp = raw * 0.0078125f;

    // ID
    if (HAL_I2C_Mem_Read(i2c, TMP117_ADDR, TMP117_REG_ID,
                         I2C_MEMADD_SIZE_8BIT, buf, 2, 100) != HAL_OK)
        return HAL_ERROR;

    *id = (buf[0] << 8) | buf[1];

    return HAL_OK;
}
