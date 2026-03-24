/*
 * Utils.c
 *
 *  Created on: 24 mar 2026
 *      Author: bartkepl
 */


#include <Utils.h>
#include <stm32c0xx_hal.h>



uint8_t SysTimTestTimer1ms_u16(uint16_t *pusCnt, uint16_t usTime) {
  uint16_t usTick = HAL_GetTick() & 0xFFFF;
  uint16_t usTickDiff = 0;
  if (usTick >= (*pusCnt)) {
    usTickDiff = (uint16_t) ((usTick - (*pusCnt)) & 0xffff);
  } else {
    usTickDiff = (uint16_t) ((UINT16_MAX - (*pusCnt) + usTick + 1) & 0xffff);
  }

  if (usTickDiff >= usTime) {
    SysTimZeroTimer1ms_u16(pusCnt);
    return 1;
  }
  return 0;
}


void SysTimZeroTimer1ms_u16(uint16_t *pusCnt) {
  (*pusCnt) = (uint16_t) (HAL_GetTick() & 0xFFFF);
}
