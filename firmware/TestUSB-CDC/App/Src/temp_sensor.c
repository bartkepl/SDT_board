/*
 * temp_sensor.c
 *
 *  Created on: 16 mar 2026
 *      Author: bartkepl
 */


#include "temp_sensor.h"
#include <dlr2416.h>
#include <math.h>

#define TMP117_ADDR  (0x48 << 1)
#define SHT45_ADDR   (0x45 << 1)

#define TMP117_REG_TEMP 0x00
#define TMP117_REG_ID   0x0F

#define SHT45_CMD_MEASURE 0xFD

TempSensor tempSensors[TEMP_SENSOR_MAX];
uint8_t tempSensorCount = 0;

static I2C_HandleTypeDef *i2c;

/* ================= TMP117 ================= */

static uint8_t TMP117_Detect(uint8_t addr)
{
    uint8_t reg = TMP117_REG_ID;
    uint8_t data[2];

    if(HAL_I2C_Master_Transmit(i2c, addr, &reg, 1, 50) != HAL_OK)
        return 0;

    if(HAL_I2C_Master_Receive(i2c, addr, data, 2, 50) != HAL_OK)
        return 0;

    return 1;
}

static float TMP117_ReadTemp(uint8_t addr)
{
    uint8_t reg = TMP117_REG_TEMP;
    uint8_t data[2];

    HAL_I2C_Master_Transmit(i2c, addr, &reg, 1, 50);
    HAL_I2C_Master_Receive(i2c, addr, data, 2, 50);

    int16_t raw = (data[0] << 8) | data[1];

    return raw * 0.0078125f;
}

/* ================= SHT45 ================= */

static uint8_t SHT45_Detect(uint8_t addr)
{
    return HAL_I2C_IsDeviceReady(i2c, addr, 3, 50) == HAL_OK;
}

static float SHT45_ReadTemp(uint8_t addr)
{
    uint8_t cmd = SHT45_CMD_MEASURE;
    uint8_t data[6];

    HAL_I2C_Master_Transmit(i2c, addr, &cmd, 1, 50);

    HAL_Delay(10);

    HAL_I2C_Master_Receive(i2c, addr, data, 6, 50);

    uint16_t raw = (data[0] << 8) | data[1];

    float temp = -45 + 175 * ((float)raw / 65535.0f);

    return temp;
}

/* ================= INIT ================= */

HAL_StatusTypeDef TempSensors_Init(I2C_HandleTypeDef *hi2c)
{
    i2c = hi2c;
    tempSensorCount = 0;

    if(TMP117_Detect(TMP117_ADDR))
    {
        tempSensors[tempSensorCount].type = TEMP_TMP117;
        tempSensors[tempSensorCount].address = TMP117_ADDR;
        tempSensorCount++;
    }

    if(SHT45_Detect(SHT45_ADDR))
    {
        tempSensors[tempSensorCount].type = TEMP_SHT45;
        tempSensors[tempSensorCount].address = SHT45_ADDR;
        tempSensorCount++;
    }

    return HAL_OK;
}

/* ================= READ ================= */

float TempSensor_Read(uint8_t id)
{
    if(id >= tempSensorCount)
        return -1000;

    TempSensor *s = &tempSensors[id];

    switch(s->type)
    {
        case TEMP_TMP117:
            return TMP117_ReadTemp(s->address);

        case TEMP_SHT45:
            return SHT45_ReadTemp(s->address);

        default:
            return -1000;
    }
}

void Display_ShowTemperature(uint8_t sensorID)
{
	static float lastTemp = -999;
    float t = TempSensor_Read(sensorID);
    char buf[9];

    char sign = '+';
    if(t < 0)
    {
        sign = '-';
        t = -t;
    }

    int whole;
    int frac;

    if(t < 10.0f)
    {
        whole = (int)t;
        frac = (int)((t - whole) * 1000);

        buf[0] = sign;
        buf[1] = '0' + whole;
        buf[2] = '.';
        buf[3] = '0' + (frac / 100);
        buf[4] = '0' + ((frac / 10) % 10);
        buf[5] = '0' + (frac % 10);
    }
    else
    {
        whole = (int)t;
        frac = (int)((t - whole) * 100);

        buf[0] = sign;
        buf[1] = '0' + (whole / 10);
        buf[2] = '0' + (whole % 10);
        buf[3] = '.';
        buf[4] = '0' + (frac / 10);
        buf[5] = '0' + (frac % 10);
    }

    buf[6] = ' ';
    buf[7] = DEGREE_CHAR;
    buf[8] = 0;

    if(fabs(t - lastTemp) > 0.01f)
    {
        lastTemp = t;
        DLR2416_WriteString8(buf);
    }
}
