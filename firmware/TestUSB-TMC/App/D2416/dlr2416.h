/*
 * dlr2416.h
 *
 *  Created on: 16 mar 2026
 *      Author: bartkepl
 */

#ifndef INC_DLR2416_H_
#define INC_DLR2416_H_

#include "main.h"

#define DISP1 0
#define DISP2 1

#define DEGREE_CHAR 0x1B

void DLR2416_Init(void);
void DLR2416_Clear(uint8_t disp);
void DLR2416_ClearAll(void);

void DLR2416_WriteChar(uint8_t disp, uint8_t pos, char c);
void DLR2416_WriteString(uint8_t disp, const char *str);
void DLR2416_WriteString8(const char *str);

void DLR2416_SetBrightness(uint8_t percent);
void DLR2416_PWM_Enable(void);
void DLR2416_PWM_Disable(GPIO_PinState state);

#endif /* INC_DLR2416_H_ */
