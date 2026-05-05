/*
 * tmp117.h
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#ifndef SENSOR_TMP117_H_
#define SENSOR_TMP117_H_

#include "main.h"

#define TMP117_ADDR (0x48 << 1)

// TMP117 Registers
#define TMP117_REG_TEMP         0x00
#define TMP117_REG_CONFIG       0x01
#define TMP117_REG_THIGH        0x02
#define TMP117_REG_TLOW         0x03
#define TMP117_REG_ID           0x0F

// TMP117 State Machine
typedef enum {
    TMP117_STATE_IDLE = 0,
    TMP117_STATE_REQ_TEMP,
    TMP117_STATE_REC_TEMP,
    TMP117_STATE_REQ_ID,
    TMP117_STATE_REC_ID,
    TMP117_STATE_REQ_CONFIG,
    TMP117_STATE_REC_CONFIG,
    TMP117_STATE_ERROR,
} TMP117_State_t;

// TMP117 Data structure
typedef struct {
    float fTemp;
    uint16_t usId;
    uint16_t usConfigRegister;
    uint8_t ucValidFlag;
    uint8_t ucNewDataFlag;
    uint8_t ucInitializedFlag;
} TMP117_Data_t;

extern TMP117_Data_t g_tmp117;

void TMP117_Init(I2C_HandleTypeDef *hi2c);
void TMP117_Task(void);
void TMP117_I2C_Complete_Callback(void);
void TMP117_I2C_Error_Callback(void);

#endif /* SENSOR_TMP117_H_ */
