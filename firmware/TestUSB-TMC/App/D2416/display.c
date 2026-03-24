/*
 * display.c
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#include <stdio.h>
#include <string.h>
#include <display.h>
#include <dlr2416.h>
#include <sensor.h>
#include <Utils.h>

#define DISPLAY_PERIOD_MS 100

static uint16_t sTaskTimer = 0;

DisplayData_t xDisplayData = {0};


void Display_Init(void){
	xDisplayData = (DisplayData_t)DISPLAY_DATA_DEFAULT;
	DLR2416_Init();
	DLR2416_ClearAll();
}

void ConvertFloatTempToChar(float t, char *buf)
{
    int temp = (int)(t * 1000.0f + (t >= 0 ? 0.5f : -0.5f)); // rounding

    char sign = ' ';
    if (temp < 0)
    {
        sign = '-';
        temp = -temp;
    }

    int whole = temp / 1000;
    int frac  = temp % 1000;

    buf[0] = sign;

    if (whole < 10)
    {
        buf[1] = '0' + whole;
        buf[2] = '.';
        buf[3] = '0' + (frac / 100);
        buf[4] = '0' + ((frac / 10) % 10);
        buf[5] = '0' + (frac % 10);
    }
    else
    {
        buf[1] = '0' + (whole / 10);
        buf[2] = '0' + (whole % 10);
        buf[3] = '.';
        buf[4] = '0' + (frac / 100);
        buf[5] = '0' + ((frac / 10) % 10);
    }

    buf[6] = ' ';
    buf[7] = DEGREE_CHAR;
}

void Display_SetMeasurement(char *data)
{
	memcpy(xDisplayData.measBuffer,data,8);
    xDisplayData.measNewData = 1;
}

void Display_SetText(char *data)
{
	memcpy(xDisplayData.textBuffer,data,8);
    xDisplayData.textNewData = 1;
}

void Display_SelectSource(DisplaySource_t src)
{
    xDisplayData.activeSource = src;
    if(src == eDisplaySource_Meas){
    	xDisplayData.measNewData = 1;
    } else if (src == eDisplaySource_Text) {
    	xDisplayData.textNewData = 1;
    }

}

const char* Display_GetText(void)
{
    return xDisplayData.textBuffer;
}

DisplaySource_t Display_GetSource(void)
{
    return xDisplayData.activeSource;
}

void Display_SetBrightness(uint8_t percent)
{
    xDisplayData.dispBrightness = percent;

    xDisplayData.newBrightnessFlag = 1;
}

void Display_SetState(uint8_t state)
{
    xDisplayData.dispActive = state;

    xDisplayData.newStateFlag = 1;
}


uint8_t Display_GetBrightness(void)
{
    return xDisplayData.dispBrightness;
}

uint8_t Display_GetState(void)
{
    return xDisplayData.dispActive;
}

void Display_task(void){
	if (!SysTimTestTimer1ms_u16(&sTaskTimer, DISPLAY_PERIOD_MS))
			return;

	switch (xDisplayData.activeSource) {
	case eDisplaySource_Meas:
		if (xDisplayData.measNewData) {
			DLR2416_WriteString8(xDisplayData.measBuffer);
			xDisplayData.measNewData = 0;
		}
		break;

	case eDisplaySource_Text:
		if (xDisplayData.textNewData) {
			DLR2416_WriteString8(xDisplayData.textBuffer);
			xDisplayData.textNewData = 0;
		}
		break;

	default:
		break;
	}

	if (xDisplayData.newBrightnessFlag) {
		DLR2416_SetBrightness(xDisplayData.dispBrightness);
		xDisplayData.newBrightnessFlag = 0;
	}
	if (xDisplayData.newStateFlag) {
		if(xDisplayData.dispActive){
			DLR2416_PWM_Enable();
		} else {
			DLR2416_PWM_Disable(GPIO_PIN_RESET);
		}
		xDisplayData.newStateFlag = 0;
	}
}

void DisplayOff(void){
	DLR2416_PWM_Disable(GPIO_PIN_RESET);
}

void DisplayClearAll(void){
	DLR2416_ClearAll();
}
