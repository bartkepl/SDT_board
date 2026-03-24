/*
 * flash.h
 *
 *  Created on: 20 mar 2026
 *      Author: bartkepl
 */

#ifndef FLASH_FLASH_H_
#define FLASH_FLASH_H_

#include "stm32c0xx_hal.h"
#include <string.h>
#include <stdint.h>

#define FLASH_CONFIG_SIZES_PAGES	1			// Ilość stron na parametry
#define FLASH_PAGES       			64        	// Ilośc stron FLASH w STM32C071
#define FLASH_PAGE_SIZE         	2048       	// rozmiar strony (2 KB dla STM32C071)
#define FLASH_CONFIG_ADDRESS    	(FLASH_BASE + (FLASH_PAGES - FLASH_CONFIG_SIZES_PAGES) * FLASH_PAGE_SIZE) // Start pamięci CONFIG FLASH

// Przykładowa struktura konfiguracji
typedef struct __attribute__((packed)) {
    uint32_t param1;
    uint16_t param2;
    uint8_t  param3;
    uint32_t crc;      // CRC32 całego bloku (bez tego pola podczas zapisu)
} ConfigType_t;


extern ConfigType gConfig;

// Funkcje
HAL_StatusTypeDef Config_Flash_Write(ConfigType* cfg);
HAL_StatusTypeDef Config_Flash_Read(ConfigType* cfg);
uint32_t Config_CalcCRC(ConfigType* cfg);

#endif /* FLASH_FLASH_H_ */
