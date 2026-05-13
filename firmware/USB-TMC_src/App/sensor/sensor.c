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

volatile uint8_t g_i2c_active_sensor = I2C_SENSOR_NONE;

static I2C_HandleTypeDef *sensor_i2c;
static uint16_t sTaskSensorTimer = 0;

static volatile SensorError_t s_pending_error = SENSOR_ERR_NONE;
static uint8_t s_tmp117_was_valid = 0;
static uint8_t s_sht45_was_valid  = 0;

static void Sensor_SetPendingError(SensorError_t err) {
    if (s_pending_error == SENSOR_ERR_NONE)
        s_pending_error = err;
}

SensorError_t Sensor_GetAndClearError(void) {
    SensorError_t err = s_pending_error;
    s_pending_error = SENSOR_ERR_NONE;
    return err;
}

#define SENSOR_PERIOD_MS 1000
#define SENSOR_DETECT_TIMEOUT_MS 5000

//------------------------------------------------------------------//
// Private functions
//------------------------------------------------------------------//

static void detectSensor(void)
{
    uint8_t found_tmp117 = (HAL_I2C_IsDeviceReady(sensor_i2c, TMP117_ADDR, 1, 5) == HAL_OK);
    uint8_t found_sht45  = (HAL_I2C_IsDeviceReady(sensor_i2c, SHT45_ADDR,  1, 5) == HAL_OK);

    if (found_tmp117 && found_sht45) {
        g_sensor.type = SENSOR_DUAL;
        TMP117_Init(sensor_i2c);
        SHT45_Init(sensor_i2c);
    } else if (found_tmp117) {
        g_sensor.type = SENSOR_TMP117;
        TMP117_Init(sensor_i2c);
    } else if (found_sht45) {
        g_sensor.type = SENSOR_SHT45;
        SHT45_Init(sensor_i2c);
    } else {
        g_sensor.type = SENSOR_NONE;
    }
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
            Sensor_SetPendingError(SENSOR_ERR_NOT_FOUND);
        }
        
        return;
    }

    // Run sensor-specific tasks
    switch (g_sensor.type)
    {
        case SENSOR_TMP117:
        {
            TMP117_Task();

            if (s_tmp117_was_valid && !g_tmp117.ucValidFlag && g_tmp117.ucInitializedFlag)
                Sensor_SetPendingError(SENSOR_ERR_COMM);
            s_tmp117_was_valid = g_tmp117.ucValidFlag;

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

            if (s_sht45_was_valid && !g_sht45.ucValidFlag && g_sht45.ucInitializedFlag)
                Sensor_SetPendingError(SENSOR_ERR_DATA);
            s_sht45_was_valid = g_sht45.ucValidFlag;

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
            if (s_tmp117_was_valid && !g_tmp117.ucValidFlag && g_tmp117.ucInitializedFlag)
                Sensor_SetPendingError(SENSOR_ERR_COMM);
            s_tmp117_was_valid = g_tmp117.ucValidFlag;

            SHT45_Task();
            if (s_sht45_was_valid && !g_sht45.ucValidFlag && g_sht45.ucInitializedFlag)
                Sensor_SetPendingError(SENSOR_ERR_DATA);
            s_sht45_was_valid = g_sht45.ucValidFlag;

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
        Display_SetMeasurement(buf, 8);
        g_sensor.ucNewDataFlag = false;
    }
}

void Sensor_I2C_TxComplete_Callback(void)
{
    switch (g_sensor.type)
    {
        case SENSOR_TMP117:
            TMP117_I2C_TxComplete_Callback();
            break;

        case SENSOR_SHT45:
            SHT45_I2C_TxComplete_Callback();
            break;

        case SENSOR_DUAL:
            if (g_i2c_active_sensor == I2C_SENSOR_TMP117)
                TMP117_I2C_TxComplete_Callback();
            else if (g_i2c_active_sensor == I2C_SENSOR_SHT45)
                SHT45_I2C_TxComplete_Callback();
            break;

        default:
            break;
    }
}

void Sensor_I2C_RxComplete_Callback(void)
{
    switch (g_sensor.type)
    {
        case SENSOR_TMP117:
            TMP117_I2C_RxComplete_Callback();
            break;

        case SENSOR_SHT45:
            SHT45_I2C_RxComplete_Callback();
            break;

        case SENSOR_DUAL:
            if (g_i2c_active_sensor == I2C_SENSOR_TMP117)
                TMP117_I2C_RxComplete_Callback();
            else if (g_i2c_active_sensor == I2C_SENSOR_SHT45)
                SHT45_I2C_RxComplete_Callback();
            break;

        default:
            break;
    }
}

void Sensor_I2C_Error_Callback(void)
{
    switch (g_sensor.type)
    {
        case SENSOR_TMP117:
            TMP117_I2C_Error_Callback();
            break;

        case SENSOR_SHT45:
            SHT45_I2C_Error_Callback();
            break;

        case SENSOR_DUAL:
            if (g_i2c_active_sensor == I2C_SENSOR_TMP117)
                TMP117_I2C_Error_Callback();
            else if (g_i2c_active_sensor == I2C_SENSOR_SHT45)
                SHT45_I2C_Error_Callback();
            break;

        default:
            break;
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

// SHT45 configuration management functions (wrappers for SCPI control)
void Sensor_SHT45_SetReadPeriod(uint16_t periodMs)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        SHT45_SetReadPeriod(periodMs);
    }
}

uint16_t Sensor_SHT45_GetReadPeriod(void)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        return SHT45_GetReadPeriod();
    }
    return 500;  // Default value if sensor is not SHT45
}

void Sensor_SHT45_SetAverageCount(uint8_t count)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        SHT45_SetAverageCount(count);
    }
}

uint8_t Sensor_SHT45_GetAverageCount(void)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        return SHT45_GetAverageCount();
    }
    return 1;  // Default value if sensor is not SHT45
}

void Sensor_SHT45_SetMeasurementPrecision(uint8_t precision)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        SHT45_SetPrecision((SHT45_Precision_t)precision);
    }
}

uint8_t Sensor_SHT45_GetMeasurementPrecision(void)
{
    if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
    {
        return (uint8_t)SHT45_GetPrecision();
    }
    return SHT45_PRECISION_HIGH;  // Default value if sensor is not SHT45
}

//------------------------------------------------------------------//
// TMP117 wrapper functions
//------------------------------------------------------------------//

#define TMP117_PRESENT (g_sensor.type == SENSOR_TMP117 || g_sensor.type == SENSOR_DUAL)

void Sensor_TMP117_SetMode(uint8_t mode)
{
    if (TMP117_PRESENT) TMP117_SetMode((TMP117_Mode_t)mode);
}
uint8_t Sensor_TMP117_GetMode(void)
{
    if (TMP117_PRESENT) return (uint8_t)TMP117_GetMode();
    return (uint8_t)TMP117_MODE_CONTINUOUS;
}

void Sensor_TMP117_SetConvRate(uint8_t rate)
{
    if (TMP117_PRESENT) TMP117_SetConvRate(rate);
}
uint8_t Sensor_TMP117_GetConvRate(void)
{
    if (TMP117_PRESENT) return TMP117_GetConvRate();
    return 4u;
}

void Sensor_TMP117_SetAvgHW(uint8_t avg)
{
    if (TMP117_PRESENT) TMP117_SetAvgHW((TMP117_Averaging_t)avg);
}
uint8_t Sensor_TMP117_GetAvgHW(void)
{
    if (TMP117_PRESENT) return (uint8_t)TMP117_GetAvgHW();
    return (uint8_t)TMP117_AVG_8;
}

void Sensor_TMP117_SetReadPeriod(uint16_t periodMs)
{
    if (TMP117_PRESENT) TMP117_SetReadPeriod(periodMs);
}
uint16_t Sensor_TMP117_GetReadPeriod(void)
{
    if (TMP117_PRESENT) return TMP117_GetReadPeriod();
    return 1000u;
}

void Sensor_TMP117_SetAlertHigh(float fTemp)
{
    if (TMP117_PRESENT) TMP117_SetAlertHigh(fTemp);
}
float Sensor_TMP117_GetAlertHigh(void)
{
    if (TMP117_PRESENT) return TMP117_GetAlertHigh();
    return 0.0f;
}

void Sensor_TMP117_SetAlertLow(float fTemp)
{
    if (TMP117_PRESENT) TMP117_SetAlertLow(fTemp);
}
float Sensor_TMP117_GetAlertLow(void)
{
    if (TMP117_PRESENT) return TMP117_GetAlertLow();
    return 0.0f;
}

uint8_t Sensor_TMP117_GetAlertStatus(void)
{
    if (TMP117_PRESENT) return TMP117_GetAlertStatus();
    return 0u;
}

void Sensor_TMP117_RequestSoftReset(void)
{
    if (TMP117_PRESENT) TMP117_RequestSoftReset();
}
