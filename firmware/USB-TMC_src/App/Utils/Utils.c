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

// Static buffers (initialized once)
static uint8_t serial_raw[SERIAL_RAW_LEN];
static char serial_short[9];   // 8 + '\0'
static char serial_full[41];   // 40 + '\0'
static uint8_t serial_initialized = 0;


uint8_t SysTimTestTimer1ms_u16(uint16_t *pusCnt, uint16_t usTime) {
  uint16_t usTick = (uint16_t)HAL_GetTick();
  // Unsigned subtraction wraps correctly on overflow (C standard)
  if ((uint16_t)(usTick - *pusCnt) >= usTime) {
    *pusCnt = usTick;
    return 1;
  }
  return 0;
}


void SysTimZeroTimer1ms_u16(uint16_t *pusCnt) {
  *pusCnt = (uint16_t)HAL_GetTick();
}




// FNV-1a hash (inline for speed)
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


// Convert byte array to uppercase hex string; out must hold 2*len+1 bytes
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


// One-time initialization of serial number buffers
static void serial_init_once(void)
{
    if (serial_initialized) return;

    // Collect device identity words without memcpy
    uint32_t *p = (uint32_t *)serial_raw;
    p[0] = HAL_GetDEVID();
    p[1] = HAL_GetREVID();
    p[2] = HAL_GetUIDw0();
    p[3] = HAL_GetUIDw1();
    p[4] = HAL_GetUIDw2();

    // Hash -> short serial (8 hex chars)
    uint32_t hash = fnv1a_32(serial_raw, SERIAL_RAW_LEN);

    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++)
    {
        serial_short[i] = hex[(hash >> (28 - 4 * i)) & 0xF];
    }
    serial_short[8] = '\0';

    // Full serial (40 hex chars)
    to_hex(serial_raw, SERIAL_RAW_LEN, serial_full);

    serial_initialized = 1;
}


// =====================================================
// PUBLIC API
// =====================================================

// Short serial number (8 chars)
const char *serial_get(void)
{
    serial_init_once();
    return serial_short;
}

// Full serial number (40 chars)
const char *serial_get_full(void)
{
    serial_init_once();
    return serial_full;
}
