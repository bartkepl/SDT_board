/*
 * dlr2416.c
 *
 *  Created on: 16 mar 2026
 *      Author: bartkepl
 */


#include "dlr2416.h"

#define SWAP_DISPLAYS 1
#define REVERSE_CHARS 1

extern TIM_HandleTypeDef htim3;

/* ===================== POMOCNICZE ===================== */

// zapis dla sygnałów aktywnych niskim poziomem (!)
static void writePinInv(GPIO_TypeDef* port, uint16_t pin, GPIO_PinState state)
{
    HAL_GPIO_WritePin(port, pin, (state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* ===================== DATA ===================== */

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

/* ===================== CONTROL ===================== */

static void setCU(GPIO_PinState state)
{
    // CU! → odwrócone
    writePinInv(CU_GPIO_Port, CU_Pin, state);
}

static void pulseCUE(void)
{
    HAL_GPIO_WritePin(CUE_GPIO_Port, CUE_Pin, GPIO_PIN_RESET);
    __NOP(); __NOP(); __NOP();
    HAL_GPIO_WritePin(CUE_GPIO_Port, CUE_Pin, GPIO_PIN_SET);
}

static void pulseWR(void)
{
    // WR! → aktywny LOW
    writePinInv(WR_GPIO_Port, WR_Pin, GPIO_PIN_SET);
    __NOP(); __NOP(); __NOP();
    writePinInv(WR_GPIO_Port, WR_Pin, GPIO_PIN_RESET);
}

/* ===================== DISPLAY SELECT ===================== */

static void selectDisplay(uint8_t disp)
{
    if(disp == DISP1)
    {
        // DISP1 aktywny
        writePinInv(CE1_1_GPIO_Port, CE1_1_Pin, GPIO_PIN_SET); // -> LOW
        writePinInv(CE2_1_GPIO_Port, CE2_1_Pin, GPIO_PIN_SET); // -> LOW

        // DISP2 nieaktywny
        writePinInv(CE1_2_GPIO_Port, CE1_2_Pin, GPIO_PIN_RESET); // -> HIGH
        writePinInv(CE2_2_GPIO_Port, CE2_2_Pin, GPIO_PIN_RESET); // -> HIGH
    }
    else
    {
        // DISP1 nieaktywny
        writePinInv(CE1_1_GPIO_Port, CE1_1_Pin, GPIO_PIN_RESET); // -> HIGH
        writePinInv(CE2_1_GPIO_Port, CE2_1_Pin, GPIO_PIN_RESET); // -> HIGH

        // DISP2 aktywny
        writePinInv(CE1_2_GPIO_Port, CE1_2_Pin, GPIO_PIN_SET); // -> LOW
        writePinInv(CE2_2_GPIO_Port, CE2_2_Pin, GPIO_PIN_SET); // -> LOW
    }
}

/* ===================== WRITE ===================== */

static void writeData(uint8_t addr, uint8_t data)
{
    setCU(GPIO_PIN_RESET); // DATA

    setAddress(addr);
    setData(data);

    pulseWR();
}

static void writeCommand(uint8_t cmd)
{
    setCU(GPIO_PIN_SET); // COMMAND

    setData(cmd);
    pulseCUE();
}

/* ===================== API ===================== */

void DLR2416_Init(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 600);

    // ustaw stany spoczynkowe

    writePinInv(WR_GPIO_Port, WR_Pin, GPIO_PIN_RESET);   // WR! = 1
    HAL_GPIO_WritePin(CUE_GPIO_Port, CUE_Pin, GPIO_PIN_SET);

    writePinInv(CLR_GPIO_Port, CLR_Pin, GPIO_PIN_RESET); // CLR! = 1

    setCU(GPIO_PIN_RESET); // DATA mode

    DLR2416_ClearAll();
}

void DLR2416_Clear(uint8_t disp)
{
    selectDisplay(disp);

    // CLR! → aktywny LOW
    writePinInv(CLR_GPIO_Port, CLR_Pin, GPIO_PIN_SET);
    HAL_Delay(1);
    writePinInv(CLR_GPIO_Port, CLR_Pin, GPIO_PIN_RESET);
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
    writeData(pos, (uint8_t)c);
}

void DLR2416_WriteString(uint8_t disp, const char *str)
{
    for(uint8_t i = 0; i < 4; i++)
    {
        if(str[i] == 0) break;
        DLR2416_WriteChar(disp, i, str[i]);
    }
}

void DLR2416_WriteString8(const char *str)
{
    for(uint8_t i = 0; i < 8; i++)
    {
        uint8_t disp = (i < 4) ? DISP1 : DISP2;
        uint8_t pos  = i % 4;

#if SWAP_DISPLAYS
        disp = (disp == DISP1) ? DISP2 : DISP1;
#endif

#if REVERSE_CHARS
        pos = 3 - pos;
#endif

        if(str[i])
            DLR2416_WriteChar(disp, pos, str[i]);
    }
}

void DLR2416_SetBrightness(uint8_t percent)
{
    uint16_t value = (1199 * percent) / 100;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, value);
}
