/*
 * Utils.h
 *
 *  Created on: 24 mar 2026
 *      Author: bartkepl
 */

#ifndef UTILS_UTILS_H_
#define UTILS_UTILS_H_

#include <stdint.h>

uint8_t SysTimTestTimer1ms_u16(uint16_t *pusCnt, uint16_t usTime);
void SysTimZeroTimer1ms_u16(uint16_t *pusCnt);

#endif /* UTILS_UTILS_H_ */
