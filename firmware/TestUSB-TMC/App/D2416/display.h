/*
 * display.h
 *
 *  Created on: 19 mar 2026
 *      Author: bartkepl
 */

#ifndef D2416_DISPLAY_H_
#define D2416_DISPLAY_H_

#include "main.h"

void Display_Init(void);
void Display_task(void);
void Display_SetBrightness(uint8_t percent);

#endif /* D2416_DISPLAY_H_ */
