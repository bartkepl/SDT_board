/*
 * sensor.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */


#include <sensor.h>
#include <sht45.h>
#include <tmp117.h>

SensorData_t g_sensor;

static I2C_HandleTypeDef *sensor_i2c;

static uint32_t lastTick = 0;
static bool initialized = false;

#define SENSOR_PERIOD_MS 999

void Sensor_Init(I2C_HandleTypeDef *hi2c)
{
    sensor_i2c = hi2c;

    g_sensor.type = SENSOR_NONE;
    g_sensor.valid = false;

    initialized = false;
}

void Sensor_SHT45Heater(void){
	SHT45_Heater();
}

static void detectSensor(void)
{
    // TMP117 address = 0x48 << 1
    if (HAL_I2C_IsDeviceReady(sensor_i2c, TMP117_ADDR, 2, 50) == HAL_OK)
    {
        g_sensor.type = SENSOR_TMP117;
        TMP117_Init(sensor_i2c);
        return;
    }

    // SHT45 address = 0x44 << 1
    if (HAL_I2C_IsDeviceReady(sensor_i2c, SHT45_ADDR, 2, 50) == HAL_OK)
    {
        g_sensor.type = SENSOR_SHT45;
        SHT45_Init(sensor_i2c);
        return;
    }

    g_sensor.type = SENSOR_NONE;
}

void Sensor_Task(void)
{
    if (HAL_GetTick() - lastTick < SENSOR_PERIOD_MS)
        return;

    lastTick = HAL_GetTick();

    if (!initialized)
    {
        detectSensor();
        initialized = true;
        return;
    }

    switch (g_sensor.type)
    {
        case SENSOR_TMP117:
        {
            float temp;
            uint16_t id;

            if (TMP117_Read(&temp, &id) == HAL_OK)
            {
                g_sensor.temperature = temp;
                g_sensor.id = id;
                g_sensor.humidity = 0;
                g_sensor.valid = true;
            }
            else
            {
                g_sensor.valid = false;
            }
        }
        break;

        case SENSOR_SHT45:
        {
            float temp, hum;
            uint32_t id;

            if (SHT45_Read(&temp, &hum, &id) == HAL_OK)
            {
                g_sensor.temperature = temp;
                g_sensor.humidity = hum;
                g_sensor.id = (uint16_t)(id & 0xFFFF);
                g_sensor.valid = true;
            }
            else
            {
                g_sensor.valid = false;
            }
        }
        break;

        default:
            g_sensor.valid = false;
            break;
    }
}
