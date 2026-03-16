/*
 * dlr2416.c
 *
 *  Created on: 16 mar 2026
 *      Author: bartkepl
 */


#include "dlr2416.h"

extern TIM_HandleTypeDef htim3;

static void setData(uint8_t data)
{
    HAL_GPIO_WritePin(D0_GPIO_Port, D0_Pin, (data >> 0) & 1);
    HAL_GPIO_WritePin(D1_GPIO_Port, D1_Pin, (data >> 1) & 1);
    HAL_GPIO_WritePin(D2_GPIO_Port, D2_Pin, (data >> 2) & 1);
    HAL_GPIO_WritePin(D3_GPIO_Port, D3_Pin, (data >> 3) & 1);
    HAL_GPIO_WritePin(D4_GPIO_Port, D4_Pin, (data >> 4) & 1);
    HAL_GPIO_WritePin(D5_GPIO_Port, D5_Pin, (data >> 5) & 1);
    HAL_GPIO_WritePin(D6_GPIO_Port, D6_Pin, (data >> 6) & 1);
}

static void setAddress(uint8_t addr)
{
    HAL_GPIO_WritePin(A0_GPIO_Port, A0_Pin, addr & 1);
    HAL_GPIO_WritePin(A1_GPIO_Port, A1_Pin, (addr >> 1) & 1);
}

static void selectDisplay(uint8_t disp)
{
    if(disp == DISP1)
    {
        HAL_GPIO_WritePin(CE1_1_GPIO_Port, CE1_1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(CE2_1_GPIO_Port, CE2_1_Pin, GPIO_PIN_RESET);

        HAL_GPIO_WritePin(CE1_2_GPIO_Port, CE1_2_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CE2_2_GPIO_Port, CE2_2_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(CE1_1_GPIO_Port, CE1_1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CE2_1_GPIO_Port, CE2_1_Pin, GPIO_PIN_SET);

        HAL_GPIO_WritePin(CE1_2_GPIO_Port, CE1_2_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(CE2_2_GPIO_Port, CE2_2_Pin, GPIO_PIN_RESET);
    }
}

static void writeByte(uint8_t addr, uint8_t data)
{
    setAddress(addr);
    setData(data);

    HAL_GPIO_WritePin(WR_GPIO_Port, WR_Pin, GPIO_PIN_RESET);
    __NOP();
    __NOP();
    HAL_GPIO_WritePin(WR_GPIO_Port, WR_Pin, GPIO_PIN_SET);
}

void DLR2416_Init(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

    HAL_GPIO_WritePin(WR_GPIO_Port, WR_Pin, GPIO_PIN_SET);
}

void DLR2416_Clear(uint8_t disp)
{
    selectDisplay(disp);

    HAL_GPIO_WritePin(CLR_GPIO_Port, CLR_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(CLR_GPIO_Port, CLR_Pin, GPIO_PIN_SET);
}

void DLR2416_ClearAll(void)
{
    DLR2416_Clear(DISP1);
    DLR2416_Clear(DISP2);
}

void DLR2416_WriteChar(uint8_t disp, uint8_t pos, char c)
{
    if(pos > 3) return;

    selectDisplay(disp);
    writeByte(pos, (uint8_t)c);
}

void DLR2416_WriteString(uint8_t disp, const char *str)
{
    for(uint8_t i=0;i<4;i++)
    {
        if(str[i]==0) break;
        DLR2416_WriteChar(disp,i,str[i]);
    }
}

void DLR2416_WriteString8(const char *str)
{
    for(uint8_t i=0;i<4;i++)
    {
        if(str[i])
            DLR2416_WriteChar(DISP1,i,str[i]);
    }

    for(uint8_t i=0;i<4;i++)
    {
        if(str[i+4])
            DLR2416_WriteChar(DISP2,i,str[i+4]);
    }
}

void DLR2416_SetBrightness(uint8_t percent)
{
	uint16_t value = (1199 * percent) / 100;
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, value);
}
