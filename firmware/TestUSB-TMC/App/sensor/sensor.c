/*
 * sensor.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */


#include <sensor.h>
#include <sht45.h>
#include <tmp117.h>
#include <display.h>
#include <Utils.h>

SensorData_t g_sensor ={0};

static I2C_HandleTypeDef *sensor_i2c;

static uint16_t sTaskSensorTimer = 0;

#define SENSOR_PERIOD_MS 1000


//------------------------------------------------------------------//


void Sensor_Init(I2C_HandleTypeDef *hi2c)
{
    sensor_i2c = hi2c;

    g_sensor.type = SENSOR_NONE;
    g_sensor.ucValidFlag = false;

    g_sensor.ucInitializedFlag = false;
}

void Sensor_SHT45Heater(void){
	SHT45_Heater();
}

static void detectSensor(void)
{
	// TMP117 address = 0x48 << 1
	if (HAL_I2C_IsDeviceReady(sensor_i2c, TMP117_ADDR, 2, 50) == HAL_OK) {
		g_sensor.type = SENSOR_TMP117;
		TMP117_Init(sensor_i2c);
		return;
	}

	// SHT45 address = 0x44 << 1
	if (HAL_I2C_IsDeviceReady(sensor_i2c, SHT45_ADDR, 2, 50) == HAL_OK) {
		if (g_sensor.type == SENSOR_NONE) {
			g_sensor.type = SENSOR_SHT45;
			SHT45_Init(sensor_i2c);
		} else {
			g_sensor.type = SENSOR_ERROR;
		}
		return;
	}

    g_sensor.type = SENSOR_NONE;
}

void Sensor_Task(void)
{
    if (!SysTimTestTimer1ms_u16(&sTaskSensorTimer, SENSOR_PERIOD_MS))
        return;

    if (!g_sensor.ucInitializedFlag)
    {
        detectSensor();
        g_sensor.ucInitializedFlag = true;
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
                g_sensor.fTemp = temp;
                g_sensor.usSensorId = id;
                g_sensor.fHum = 0;
                g_sensor.ucValidFlag = true;
            }
            else
            {
                g_sensor.ucValidFlag = false;
            }
        }
        break;

        case SENSOR_SHT45:
        {
            float temp, hum;
            uint32_t id;

            if (SHT45_Read(&temp, &hum, &id) == HAL_OK)
            {
                g_sensor.fTemp = temp;
                g_sensor.fHum = hum;
                g_sensor.usSensorId = (uint16_t)(id & 0xFFFF);
                g_sensor.ucValidFlag = true;
            }
            else
            {
                g_sensor.ucValidFlag = false;
            }
        }
        break;

        default:
            g_sensor.ucValidFlag = false;
            break;
    }

    if(g_sensor.ucValidFlag){
    	char buf[8];
    	ConvertFloatTempToChar(g_sensor.fTemp, buf);
    	Display_SetMeasurement(buf);
    }
}
