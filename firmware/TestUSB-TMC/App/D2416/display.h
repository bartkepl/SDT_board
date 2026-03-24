/*
 * display.h
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#ifndef D2416_DISPLAY_H_
#define D2416_DISPLAY_H_

#include "main.h"

typedef enum {
	eDisplaySource_Meas = 0,
	eDisplaySource_Text,
	eDisplaySource_SIZE
}DisplaySource_t;

typedef struct {
    char measBuffer[8];
    char textBuffer[8];

    uint8_t measNewData;
    uint8_t textNewData;

    uint8_t dispBrightness;
    uint8_t dispActive;

    uint8_t newBrightnessFlag;
    uint8_t newStateFlag;

    DisplaySource_t activeSource;
} DisplayData_t;


#define DISPLAY_DATA_DEFAULT {            \
    .measBuffer = {'-','-','-','-','-','-','-','-'}, \
    .textBuffer = {'-','-','-','-','-','-','-','-'}, \
    .measNewData = 1,                     \
    .textNewData = 0,                     \
    .dispBrightness = 4,                  \
    .dispActive = 1,                      \
    .newBrightnessFlag = 1,               \
    .newStateFlag = 1,                    \
    .activeSource = eDisplaySource_Meas   \
}

void Display_Init(void);
void Display_task(void);
void Display_SetBrightness(uint8_t percent);
void ConvertFloatTempToChar(float t, char *buf);
void Display_SetState(uint8_t state);
void DisplayClearAll(void);

void Display_SetMeasurement(char *data);
void Display_SetText(char *data);
void Display_SelectSource(DisplaySource_t src);
void DisplayOff(void);
const char* Display_GetText(void);
DisplaySource_t Display_GetSource(void);

uint8_t Display_GetBrightness(void);
uint8_t Display_GetState(void);

#endif /* D2416_DISPLAY_H_ */
