/*
 * sht45.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */


#include <sht45.h>
#include <Utils.h>

static I2C_HandleTypeDef *i2c;

SensorSht45Data_t xSensorSht45Data = {};
uint16_t sSht45TaskTimer = 0;
uint16_t sSht45SenseTimer = 0;
Sht45SensorState_t eSht45SensorState =0;

static uint8_t crc8(uint8_t *data, int len)
{
    uint8_t crc = 0xFF;

    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

HAL_StatusTypeDef SHT45_Init(I2C_HandleTypeDef *hi2c)
{
	//xSensorSht45Data.eMeasPrec = ePrecMeas_High;
    i2c = hi2c;
    return HAL_OK;
}

HAL_StatusTypeDef SHT45_Heater(void)
{
	uint8_t buf[6];
	uint8_t cmd = 0x2F;

	if (HAL_I2C_Master_Transmit(i2c, SHT45_ADDR, &cmd, 1, 100) != HAL_OK)
        return HAL_ERROR;

    HAL_Delay(1100);

    if (HAL_I2C_Master_Receive(i2c, SHT45_ADDR, buf, 6, 100) != HAL_OK)
        return HAL_ERROR;
    return HAL_OK;
}

HAL_StatusTypeDef SHT45_Read(float *temp, float *hum, uint32_t *id)
{


    uint8_t cmd[1] = {0xFD}; // high precision
    uint8_t buf[6];

    if (HAL_I2C_Master_Transmit(i2c, SHT45_ADDR, cmd, 1, 100) != HAL_OK)
        return HAL_ERROR;

    HAL_Delay(10);

    if (HAL_I2C_Master_Receive(i2c, SHT45_ADDR, buf, 6, 100) != HAL_OK)
        return HAL_ERROR;

    // CRC check
    if (crc8(buf, 2) != buf[2]) return HAL_ERROR;
    if (crc8(buf+3, 2) != buf[5]) return HAL_ERROR;

    uint16_t rawT = (buf[0] << 8) | buf[1];
    uint16_t rawH = (buf[3] << 8) | buf[4];

    *temp = -45 + 175 * ((float)rawT / 65535.0f);
    *hum  = 100 * ((float)rawH / 65535.0f);

    // ID

    cmd[0] = 0x89; // ID

	if (HAL_I2C_Master_Transmit(i2c, SHT45_ADDR, cmd, 1, 100) != HAL_OK)
		return HAL_ERROR;

	HAL_Delay(10);

	if (HAL_I2C_Master_Receive(i2c, SHT45_ADDR, buf, 6, 100) != HAL_OK)
		return HAL_ERROR;

	// CRC check
	if (crc8(buf, 2) != buf[2]) return HAL_ERROR;
	if (crc8(buf + 3, 2) != buf[5]) return HAL_ERROR;

	*id = (buf[0] << 24) | (buf[1] << 16) | (buf[3] << 8) | buf[4];


    return HAL_OK;
}


void SHT45_Task(void){
	if(!SysTimTestTimer1ms_u16(&sSht45TaskTimer, 10)) return;

	switch(eSht45SensorState)
	{
	case eSht45SensorState_Init:
		eSht45SensorState = eSht45SensorState_Wait;
		break;
	case eSht45SensorState_Wait:
		if (sSht45SenseTimer) {
			eSht45SensorState = eSht45SensorState_RequestMeas;
			break;
		}
		if (xSensorSht45Data.cHeaterActivationFlag) {
			eSht45SensorState = eSht45SensorState_RequestHeater;
			break;
		}
		if (xSensorSht45Data.cSoftResetFlag) {
			eSht45SensorState = eSht45SensorState_RequestSoftReset;
			break;
		}
		break;
	case eSht45SensorState_RequestSoftReset:
		if (HAL_I2C_Master_Transmit(i2c, SHT45_ADDR, 0x94, 1, 100) != HAL_OK){
			eSht45SensorState = eeSht45SensorState_Error;
		}
		SysTimZeroTimer1ms_u16(&sSht45TaskTimer);
		eSht45SensorState = eSht45SensorState_Init;
		break;
	case eSht45SensorState_RequestMeas:
	case eSht45SensorState_RequestHeater:
		break;
	default:
		Error_Handler();
	}
}
