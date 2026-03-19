/*
 * display.c
 *
 *  Created on: 19 mar 2026
 *      Author: bniemiec
 */

#include <display.h>
#include <dlr2416.h>

#define DISPLAY_PERIOD_MS 1000

static uint32_t lastTick = 0;

void Display_Init(void){
	DLR2416_Init();
	DLR2416_ClearAll();
	DLR2416_SetBrightness(50);
}


void Display_task(void){
	if (HAL_GetTick() - lastTick < DISPLAY_PERIOD_MS)
	        return;
	    lastTick = HAL_GetTick();

	    DLR2416_WriteString8("  Test  ");

}
