/*
 * Utils.c
 *
 *  Created on: 24 mar 2026
 *      Author: bartkepl
 */


#include <Utils.h>
#include <stm32c0xx_hal.h>
#include <stdint.h>
#include <stddef.h>

#define SERIAL_RAW_LEN 20

// 🔹 Bufory statyczne (inicjalizowane raz)
static uint8_t serial_raw[SERIAL_RAW_LEN];
static char serial_short[9];   // 8 + '\0'
static char serial_full[41];   // 40 + '\0'
static uint8_t serial_initialized = 0;


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




// 🔹 FNV-1a (inline = szybciej)
static inline uint32_t fnv1a_32(const uint8_t *data, size_t len)
{
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < len; i++)
    {
        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}


// 🔹 Zamiana na HEX (uniwersalna)
static void to_hex(const uint8_t *in, size_t len, char *out)
{
    static const char hex[] = "0123456789ABCDEF";

    for (size_t i = 0; i < len; i++)
    {
        out[2 * i]     = hex[in[i] >> 4];
        out[2 * i + 1] = hex[in[i] & 0x0F];
    }

    out[2 * len] = '\0';
}


// 🔹 Inicjalizacja (wywoływana automatycznie)
static void serial_init_once(void)
{
    if (serial_initialized) return;

    // 🔸 zbieranie danych BEZ memcpy
    uint32_t *p = (uint32_t *)serial_raw;
    p[0] = HAL_GetDEVID();
    p[1] = HAL_GetREVID();
    p[2] = HAL_GetUIDw0();
    p[3] = HAL_GetUIDw1();
    p[4] = HAL_GetUIDw2();

    // 🔸 hash → short serial
    uint32_t hash = fnv1a_32(serial_raw, SERIAL_RAW_LEN);

    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++)
    {
        serial_short[i] = hex[(hash >> (28 - 4 * i)) & 0xF];
    }
    serial_short[8] = '\0';

    // 🔸 pełny serial (40 znaków)
    to_hex(serial_raw, SERIAL_RAW_LEN, serial_full);

    serial_initialized = 1;
}


// =====================================================
// 🔹 PUBLIC API
// =====================================================

// krótki (8 znaków)
const char *serial_get(void)
{
    serial_init_once();
    return serial_short;
}

// pełny (40 znaków)
const char *serial_get_full(void)
{
    serial_init_once();
    return serial_full;
}
