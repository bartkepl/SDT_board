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

#define DISPLAY_PERIOD_MS       100
#define DISPLAY_ERR_QUEUE_SIZE  4
#define DISPLAY_ERR_SHOW_TICKS  15   /* 15 × 100ms = 1500ms */

static uint16_t        sTaskTimer = 0;

DisplayData_t xDisplayData = {0};

static int16_t         s_err_queue[DISPLAY_ERR_QUEUE_SIZE];
static uint8_t         s_err_head         = 0;
static uint8_t         s_err_count        = 0;
static uint8_t         s_err_ticks        = 0;
static DisplaySource_t s_err_prev_source  = eDisplaySource_Meas;


void Display_Init(void){
	xDisplayData = (DisplayData_t)DISPLAY_DATA_DEFAULT;
	DLR2416_Init();
	Display_SetBrightness(xDisplayData.dispBrightness);
	DLR2416_ClearAll();
}

void ConvertFloatTempToChar(float t, char *buf)
{
    /* Operational range: -55..+99 °C. Outside this range display a label instead
     * of a numeric value — temperatures this extreme indicate a hardware fault. */
    if (t >= 100.0f) {
        memcpy(buf, " OvRng  ", 8);
        return;
    }
    if (t < -55.0f) {
        memcpy(buf, " Undrng ", 8);
        return;
    }

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

void Display_SetMeasurement(const char *data, size_t len)
{
    size_t n = len < 8u ? len : 8u;
    memset(xDisplayData.measBuffer, ' ', 8);
    memcpy(xDisplayData.measBuffer, data, n);
    xDisplayData.measNewData = 1;
}

void Display_SetText(const char *data, size_t len)
{
    size_t n = len < 8u ? len : 8u;
    memset(xDisplayData.textBuffer, ' ', 8);
    memcpy(xDisplayData.textBuffer, data, n);
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

static void _err_show_next(void) {
    char buf[9];
    int16_t code = s_err_queue[s_err_head];
    s_err_head = (s_err_head + 1u) % DISPLAY_ERR_QUEUE_SIZE;
    s_err_count--;
    snprintf(buf, sizeof(buf), "ERR:%-4d", (int)code);
    DLR2416_WriteString8(buf);
    s_err_ticks = DISPLAY_ERR_SHOW_TICKS;
}

void Display_task(void) {
    if (!SysTimTestTimer1ms_u16(&sTaskTimer, DISPLAY_PERIOD_MS)) return;

    if (s_err_ticks > 0) {
        s_err_ticks--;
        if (s_err_ticks == 0) {
            if (s_err_count > 0) {
                _err_show_next();
            } else {
                xDisplayData.activeSource = s_err_prev_source;
                if (s_err_prev_source == eDisplaySource_Meas)
                    xDisplayData.measNewData = 1;
                else
                    xDisplayData.textNewData = 1;
            }
        }
    } else {
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
        default: break;
        }
    }

    if (xDisplayData.newBrightnessFlag) {
        DLR2416_SetBrightness(xDisplayData.dispBrightness);
        xDisplayData.newBrightnessFlag = 0;
    }
    if (xDisplayData.newStateFlag) {
        if (xDisplayData.dispActive)
            DLR2416_PWM_Enable();
        else
            DLR2416_PWM_Disable(GPIO_PIN_RESET);
        xDisplayData.newStateFlag = 0;
    }
}

void Display_ShowError(int16_t code) {
    if (s_err_count < DISPLAY_ERR_QUEUE_SIZE) {
        uint8_t tail = (s_err_head + s_err_count) % DISPLAY_ERR_QUEUE_SIZE;
        s_err_queue[tail] = code;
        s_err_count++;
    }
    if (s_err_ticks == 0) {
        s_err_prev_source = xDisplayData.activeSource;
        _err_show_next();
    }
}

void DisplayOff(void){
	DLR2416_PWM_Disable(GPIO_PIN_RESET);
}

void DisplayClearAll(void){
	DLR2416_ClearAll();
}
