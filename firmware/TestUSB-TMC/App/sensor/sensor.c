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

SensorData_t g_sensor = {0};

static I2C_HandleTypeDef *sensor_i2c;
static uint16_t sTaskSensorTimer = 0;

#define SENSOR_PERIOD_MS 1000
#define SENSOR_DETECT_TIMEOUT_MS 5000

//------------------------------------------------------------------//
// Private functions
//------------------------------------------------------------------//

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
			g_sensor.type = SENSOR_DUAL;
		}
		return;
	}

    g_sensor.type = SENSOR_NONE;
}

//------------------------------------------------------------------//
// Public functions
//------------------------------------------------------------------//

void Sensor_Init(I2C_HandleTypeDef *hi2c)
{
    sensor_i2c = hi2c;

    g_sensor.type = SENSOR_NONE;
    g_sensor.ucValidFlag = false;
    g_sensor.ucNewDataFlag = false;
    g_sensor.ucInitializedFlag = false;
    
    g_sensor.fTemp = 0;
    g_sensor.fHum = 0;
    g_sensor.usSensorId = 0;
    
    SysTimZeroTimer1ms_u16(&sTaskSensorTimer);
}

void Sensor_Task(void)
{
    // Sensor detection phase
    if (!g_sensor.ucInitializedFlag)
    {
        if (SysTimTestTimer1ms_u16(&sTaskSensorTimer, 100))
        {
            detectSensor();
        }
        
        if (g_sensor.type != SENSOR_NONE)
        {
            g_sensor.ucInitializedFlag = true;
            SysTimZeroTimer1ms_u16(&sTaskSensorTimer);
        }
        
        // Timeout after 5 seconds
        if (SysTimTestTimer1ms_u16(&sTaskSensorTimer, SENSOR_DETECT_TIMEOUT_MS))
        {
            g_sensor.ucInitializedFlag = true;
            g_sensor.type = SENSOR_ERROR;
        }
        
        return;
    }

    // Run sensor-specific tasks
    switch (g_sensor.type)
    {
        case SENSOR_TMP117:
        {
            TMP117_Task();
            
            if (g_tmp117.ucValidFlag)
            {
                g_sensor.fTemp = g_tmp117.fTemp;
                g_sensor.usSensorId = g_tmp117.usId;
                g_sensor.fHum = 0;
                g_sensor.ucValidFlag = true;
            }
            else
            {
                g_sensor.ucValidFlag = false;
            }
            
            if (g_tmp117.ucNewDataFlag)
            {
                g_sensor.ucNewDataFlag = true;
                g_tmp117.ucNewDataFlag = false;
            }
        }
        break;

        case SENSOR_SHT45:
        {
            SHT45_Task();
            
            if (g_sht45.ucValidFlag)
            {
                g_sensor.fTemp = g_sht45.fTemp;
                g_sensor.fHum = g_sht45.fHum;
                g_sensor.usSensorId = (uint32_t)g_sht45.uSerialNumber;
                g_sensor.ucValidFlag = true;
            }
            else
            {
                g_sensor.ucValidFlag = false;
            }
            
            if (g_sht45.ucNewDataFlag)
            {
                g_sensor.ucNewDataFlag = true;
                g_sht45.ucNewDataFlag = false;
            }
        }
        break;

        case SENSOR_DUAL:
        {
            TMP117_Task();
            SHT45_Task();
            
            // For dual sensor mode, use SHT45 data (has temp and humidity)
            if (g_sht45.ucValidFlag)
            {
                g_sensor.fTemp = g_sht45.fTemp;
                g_sensor.fHum = g_sht45.fHum;
                g_sensor.ucValidFlag = true;
            }
            else if (g_tmp117.ucValidFlag)
            {
                g_sensor.fTemp = g_tmp117.fTemp;
                g_sensor.fHum = 0;
                g_sensor.ucValidFlag = true;
            }
            else
            {
                g_sensor.ucValidFlag = false;
            }
            
            if (g_sht45.ucNewDataFlag || g_tmp117.ucNewDataFlag)
            {
                g_sensor.ucNewDataFlag = true;
                g_sht45.ucNewDataFlag = false;
                g_tmp117.ucNewDataFlag = false;
            }
        }
        break;

        default:
            g_sensor.ucValidFlag = false;
            break;
    }

    // Update display if new data available
    if (g_sensor.ucNewDataFlag && g_sensor.ucValidFlag)
    {
        char buf[8];
        ConvertFloatTempToChar(g_sensor.fTemp, buf);
        Display_SetMeasurement(buf);
        g_sensor.ucNewDataFlag = false;
    }
}

void Sensor_I2C_TxComplete_Callback(void)
{
    // Route callback to appropriate sensor
    switch (g_sensor.type)
    {
        case SENSOR_TMP117:
        	TMP117_I2C_TxComplete_Callback();
            break;
            
        case SENSOR_SHT45:
        case SENSOR_DUAL:
            SHT45_I2C_TxComplete_Callback();
            break;
            
        default:
            break;
    }
}

void Sensor_I2C_RxComplete_Callback(void)
{
    // Route callback to appropriate sensor
    switch (g_sensor.type)
    {
        case SENSOR_TMP117:
        	TMP117_I2C_RxComplete_Callback();
            break;
            
        case SENSOR_SHT45:
        case SENSOR_DUAL:
            SHT45_I2C_RxComplete_Callback();
            break;
            
        default:
            break;
    }
}

void Sensor_I2C_Error_Callback(void)
{
    // Route error callback to appropriate sensor(s)
    if (g_sensor.type == SENSOR_TMP117)
    {
        TMP117_I2C_Error_Callback();
    }
    else if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        SHT45_I2C_Error_Callback();
    }
}

// SHT45 specific control functions
void Sensor_SHT45_RequestHeater(uint8_t ucHeaterMode)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        SHT45_RequestHeater((SHT45_HeaterMode_t)ucHeaterMode);
    }
}

void Sensor_SHT45_RequestSoftReset(void)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        SHT45_RequestSoftReset();
    }
}

void Sensor_SHT45_SetPrecision(uint8_t ucPrecision)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        SHT45_SetMeasurementPrecision((SHT45_Precision_t)ucPrecision);
    }
}
