/*
 * display.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#include <display.h>
#include <dlr2416.h>
#include <sensor.h>

#define DISPLAY_PERIOD_MS 1000

static uint32_t lastTick = 0;

void Display_Init(void){
	DLR2416_Init();
	DLR2416_ClearAll();
	DisplayOn();
	DLR2416_SetBrightness(4);

}

void Display_ShowTemperature(float t)
{
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


    DLR2416_WriteString8(buf);
}

void Display_task(void){
	if (HAL_GetTick() - lastTick < DISPLAY_PERIOD_MS)
	        return;
	    lastTick = HAL_GetTick();

	    Display_ShowTemperature(g_sensor.temperature);

}

void Display_SetBrightness(uint8_t percent){
	DLR2416_SetBrightness(percent);
}

void DisplayOn(void){
	DLR2416_PWM_Enable();
}

void DisplayOff(void){
	DLR2416_PWM_Disable(GPIO_PIN_RESET);
}
