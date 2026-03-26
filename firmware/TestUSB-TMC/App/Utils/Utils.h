/*
 * Utils.h
 *
 *  Created on: 24 mar 2026
 *      Author: bartkepl
 */

#ifndef UTILS_UTILS_H_
#define UTILS_UTILS_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>




uint8_t SysTimTestTimer1ms_u16(uint16_t *pusCnt, uint16_t usTime);
void SysTimZeroTimer1ms_u16(uint16_t *pusCnt);

const char *serial_get(void);
const char *serial_get_full(void);


#endif /* UTILS_UTILS_H_ */
