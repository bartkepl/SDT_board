/*
 * flash.c
 *
 *  Created on: 20 mar 2026
 *      Author: bniemiec
 */

#include "flash.h"

// Globalna konfiguracja
ConfigType gConfig;

// Funkcja obliczająca CRC32 (CRC32 standard STM32)
uint32_t Config_CalcCRC(ConfigType* cfg)
{
    CRC_HandleTypeDef hcrc;
    __HAL_RCC_CRC_CLK_ENABLE();
    hcrc.Instance = CRC;

    HAL_CRC_Init(&hcrc);

    // Obliczamy CRC bez pola crc
    uint32_t crc_val = HAL_CRC_Calculate(&hcrc, (uint32_t*)cfg, (sizeof(ConfigType)-sizeof(uint32_t))/4);

    HAL_CRC_DeInit(&hcrc);
    return crc_val;
}

// Zapis do FLASH
HAL_StatusTypeDef Config_Flash_Write(ConfigType* cfg)
{
    HAL_StatusTypeDef status;

    // Ustaw CRC
    cfg->crc = Config_CalcCRC(cfg);

    // Odblokuj FLASH
    HAL_FLASH_Unlock();

    // Usuń stronę przed zapisem
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t pageError;
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.Page = FLASH_CONFIG_ADDRESS;
    eraseInit.NbPages = 1;

    status = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        return status;
    }

    // Zapisz strukturę do FLASH
    uint32_t* data = (uint32_t*)cfg;
    for (size_t i = 0; i < sizeof(ConfigType)/4; i++)
    {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FAST, FLASH_CONFIG_ADDRESS + i*4, data[i]);
        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();
            return status;
        }
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

// Odczyt z FLASH
HAL_StatusTypeDef Config_Flash_Read(ConfigType* cfg)
{
    ConfigType temp;
    memcpy(&temp, (void*)FLASH_CONFIG_ADDRESS, sizeof(ConfigType));

    // Sprawdź CRC
    uint32_t crc_calc = Config_CalcCRC(&temp);
    if (crc_calc != temp.crc)
    {
        return HAL_ERROR; // błąd CRC
    }

    memcpy(cfg, &temp, sizeof(ConfigType));
    return HAL_OK;
}
