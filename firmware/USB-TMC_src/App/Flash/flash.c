/*
 * flash.c — persistent configuration storage (DEFAULT / PRIMARY / BACKUP)
 *
 * Recovery order at boot:
 *   1. PRIMARY (magic + CRC valid)       → apply, dirty=false
 *   2. BACKUP  (magic + CRC valid)       → apply, repair PRIMARY, dirty=false
 *   3. DEFAULT or hardcoded fallback     → bootstrap PRIMARY + BACKUP, dirty=false
 */

#include "flash.h"
#include "display.h"
#include "sensor.h"
#include "sht45.h"
#include "tmp117.h"
#include "stm32c0xx_hal.h"
#include <string.h>

/* ===== Hardware CRC handle (defined in main.c) ====================== */

extern CRC_HandleTypeDef hcrc;

/* ===== DEFAULT config block — placed in FLASHEE page 56 ============= */

__attribute__((section(".config_default"), used))
const SDT_Config_t s_default_flash = {
    .magic              = CONFIG_MAGIC,
    .version            = CONFIG_VERSION,
    /* Display defaults */
    .dispBrightness     = 20u,
    .dispState          = 1u,
    .dispSource         = 0u,   /* eDisplaySource_Meas */
    ._r0                = 0u,
    /* SHT45 defaults */
    .sht45Precision     = 0xFDu, /* SHT45_PRECISION_HIGH */
    .sht45HeaterMode    = 0x1Eu, /* SHT45_HEATER_20MW_1S */
    .sht45ReadPeriodMs  = 1000u,
    .sht45AverageCount  = 1u,
    ._r1                = {0u, 0u, 0u},
    /* TMP117 defaults */
    .tmp117Mode         = 0u,    /* TMP117_MODE_CONTINUOUS */
    .tmp117ConvRate     = 4u,    /* 1 s conversion cycle */
    .tmp117Avg          = 1u,    /* TMP117_AVG_8 */
    ._r2                = 0u,
    .tmp117ReadPeriodMs = 1000u,
    .tmp117AlertHighRaw = 10240, /* 80.0 °C / 0.0078125 */
    .tmp117AlertLowRaw  = -1280, /* -10.0 °C / 0.0078125 */
    ._r3                = {0u, 0u},
    /* Calibration defaults — identity polynomial (degree 5), inactive */
    .cal_a0             = 0.0f,
    .cal_a1             = 1.0f,
    .cal_a2             = 0.0f,
    .cal_a3             = 0.0f,
    .cal_a4             = 0.0f,
    .cal_a5             = 0.0f,
    .cal_active         = 0u,
    ._r4                = 0u,
    .cal_date           = "----------",
    ._r5                = {0u, 0u, 0u, 0u},
    /* CRC intentionally 0 — DEFAULT is validated by magic + version only */
    .crc32              = 0u,
    ._pad               = 0u,
};

/* ===== PRIMARY / BACKUP placeholders — NOLOAD sections ============== *
 * These claim address space in the linker map and appear in the .map   *
 * file.  They are NOT written during firmware programming (NOLOAD),    *
 * so user config survives across firmware updates.                     */

__attribute__((section(".config_primary"), used))
static const uint8_t __primary_reserved[sizeof(SDT_Config_t)];

__attribute__((section(".config_backup"), used))
static const uint8_t __backup_reserved[sizeof(SDT_Config_t)];

/* Fallback defaults when DEFAULT flash block is missing or wrong version.
 * Uses the compile-time constant s_default_flash — single source of truth. */
#define s_fallback s_default_flash

/* ===== Global RAM copy =============================================== */

SDT_Config_t g_config;
bool         g_config_dirty = false;

/* ===== Internal helpers ============================================== */

/* CRC32 over the first 72 bytes (18 words) before the crc32 field */
static uint32_t crc32_compute(const SDT_Config_t *cfg)
{
    /* HAL_CRC_Calculate resets the CRC unit before computation */
    return HAL_CRC_Calculate(&hcrc, (uint32_t *)(uintptr_t)cfg, 18u);
}

static bool block_magic_ok(const SDT_Config_t *cfg)
{
    return cfg->magic == CONFIG_MAGIC;
}

static bool block_crc_ok(const SDT_Config_t *cfg)
{
    return block_magic_ok(cfg) && (crc32_compute(cfg) == cfg->crc32);
}

/* Read a config block from absolute flash address into dst */
static void flash_read_block(uint32_t addr, SDT_Config_t *dst)
{
    memcpy(dst, (const void *)addr, sizeof(SDT_Config_t));
}

/* Zero all reserved padding bytes in a config block */
static void clear_padding(SDT_Config_t *cfg)
{
    cfg->_r0 = 0u;
    cfg->_r1[0] = cfg->_r1[1] = cfg->_r1[2] = 0u;
    cfg->_r2 = 0u;
    cfg->_r3[0] = cfg->_r3[1] = 0u;
    cfg->_r4 = 0u;
    cfg->_r5[0] = cfg->_r5[1] = cfg->_r5[2] = cfg->_r5[3] = 0u;
    cfg->_pad = 0u;
}

/* Erase one 2 KB page and write the config block (5 × DOUBLEWORD) */
static ConfigStatus_t flash_write_block(uint32_t page, const SDT_Config_t *cfg)
{
    FLASH_EraseInitTypeDef eraseInit = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Page      = page,
        .NbPages   = 1u,
    };
    uint32_t pageError = 0u;

    HAL_FLASH_Unlock();

    if (HAL_FLASHEx_Erase(&eraseInit, &pageError) != HAL_OK) {
        HAL_FLASH_Lock();
        return CONFIG_ERR_FLASH;
    }

    uint32_t addr = FLASH_BASE + page * FLASH_PAGE_SIZE;
    const uint8_t *src = (const uint8_t *)cfg;

    for (uint32_t i = 0u; i < sizeof(SDT_Config_t); i += 8u) {
        uint64_t dword;
        memcpy(&dword, src + i, 8u);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dword) != HAL_OK) {
            HAL_FLASH_Lock();
            return CONFIG_ERR_FLASH;
        }
    }

    HAL_FLASH_Lock();
    return CONFIG_OK;
}

/* ===== Config_Apply — push g_config into hardware drivers =========== */

void Config_Apply(void)
{
    Display_SetBrightness(g_config.dispBrightness);
    Display_SetState(g_config.dispState);
    Display_SelectSource((DisplaySource_t)g_config.dispSource);

    Sensor_SHT45_SetReadPeriod(g_config.sht45ReadPeriodMs);
    Sensor_SHT45_SetAverageCount(g_config.sht45AverageCount);
    Sensor_SHT45_SetMeasurementPrecision(g_config.sht45Precision);

    Sensor_TMP117_SetMode(g_config.tmp117Mode);
    Sensor_TMP117_SetConvRate(g_config.tmp117ConvRate);
    Sensor_TMP117_SetAvgHW(g_config.tmp117Avg);
    Sensor_TMP117_SetReadPeriod(g_config.tmp117ReadPeriodMs);
    /* Alert thresholds stored as raw int16; convert through float API */
    Sensor_TMP117_SetAlertHigh((float)g_config.tmp117AlertHighRaw * 0.0078125f);
    Sensor_TMP117_SetAlertLow((float)g_config.tmp117AlertLowRaw  * 0.0078125f);
}

/* ===== Config_Capture — snapshot driver state into g_config ========= */

void Config_Capture(void)
{
    g_config.magic   = CONFIG_MAGIC;
    g_config.version = CONFIG_VERSION;

    g_config.dispBrightness = Display_GetBrightness();
    g_config.dispState      = Display_GetState();
    g_config.dispSource     = (uint8_t)Display_GetSource();

    g_config.sht45ReadPeriodMs  = Sensor_SHT45_GetReadPeriod();
    g_config.sht45AverageCount  = Sensor_SHT45_GetAverageCount();
    g_config.sht45Precision     = Sensor_SHT45_GetMeasurementPrecision();
    g_config.sht45HeaterMode    = s_default_flash.sht45HeaterMode; /* not runtime-changed */

    g_config.tmp117Mode         = Sensor_TMP117_GetMode();
    g_config.tmp117ConvRate     = Sensor_TMP117_GetConvRate();
    g_config.tmp117Avg          = Sensor_TMP117_GetAvgHW();
    g_config.tmp117ReadPeriodMs = Sensor_TMP117_GetReadPeriod();
    g_config.tmp117AlertHighRaw = (int16_t)(Sensor_TMP117_GetAlertHigh() / 0.0078125f);
    g_config.tmp117AlertLowRaw  = (int16_t)(Sensor_TMP117_GetAlertLow()  / 0.0078125f);
}

/* ===== Config_Init — boot loader with fallback + auto-bootstrap ===== */

void Config_Init(void)
{
    SDT_Config_t tmp;

    /* 1. Try PRIMARY */
    flash_read_block(CONFIG_ADDR_PRIMARY, &tmp);
    if (block_crc_ok(&tmp)) {
        g_config = tmp;
        g_config_dirty = false;
        Config_Apply();
        return;
    }

    /* 2. Try BACKUP → repair PRIMARY */
    flash_read_block(CONFIG_ADDR_BACKUP, &tmp);
    if (block_crc_ok(&tmp)) {
        g_config = tmp;
        flash_write_block(CONFIG_PAGE_PRIMARY, &tmp);
        g_config_dirty = false;
        Config_Apply();
        return;
    }

    /* 3. First boot or both regions corrupt — bootstrap from DEFAULT.
     *    Read DEFAULT (magic + version check, no CRC); fall back to hardcoded
     *    values if DEFAULT flash is missing, corrupt, or a different version.
     *    Compute proper CRC and save to PRIMARY + BACKUP so next boot
     *    takes the fast path (step 1) and dirty flag stays false.      */
    flash_read_block(CONFIG_ADDR_DEFAULT, &tmp);
    if (!block_magic_ok(&tmp) || tmp.version != CONFIG_VERSION) {
        tmp = s_fallback;
    }
    clear_padding(&tmp);
    tmp.crc32 = crc32_compute(&tmp);

    flash_write_block(CONFIG_PAGE_PRIMARY, &tmp);
    flash_write_block(CONFIG_PAGE_BACKUP,  &tmp);

    g_config = tmp;
    g_config_dirty = false;  /* bootstrapped cleanly, no user action required */
    Config_Apply();
}

/* ===== Config_Save — write RAM snapshot to PRIMARY + BACKUP ========= */

ConfigStatus_t Config_Save(void)
{
    Config_Capture();
    clear_padding(&g_config);
    g_config.crc32 = crc32_compute(&g_config);

    ConfigStatus_t st;
    st = flash_write_block(CONFIG_PAGE_PRIMARY, &g_config);
    if (st != CONFIG_OK) return st;
    st = flash_write_block(CONFIG_PAGE_BACKUP, &g_config);
    if (st != CONFIG_OK) return st;

    g_config_dirty = false;
    return CONFIG_OK;
}

/* ===== Config_Restore — copy BACKUP → PRIMARY ======================= */

ConfigStatus_t Config_Restore(void)
{
    SDT_Config_t tmp;
    flash_read_block(CONFIG_ADDR_BACKUP, &tmp);

    if (!block_crc_ok(&tmp))
        return CONFIG_ERR_CRC;

    ConfigStatus_t st = flash_write_block(CONFIG_PAGE_PRIMARY, &tmp);
    if (st != CONFIG_OK) return st;

    g_config = tmp;
    g_config_dirty = false;
    Config_Apply();
    return CONFIG_OK;
}

/* ===== Config_Recall — copy DEFAULT → PRIMARY + BACKUP ============== */

ConfigStatus_t Config_Recall(void)
{
    SDT_Config_t tmp;
    flash_read_block(CONFIG_ADDR_DEFAULT, &tmp);

    if (!block_magic_ok(&tmp) || tmp.version != CONFIG_VERSION) {
        tmp = s_fallback;
    }

    clear_padding(&tmp);
    tmp.crc32 = crc32_compute(&tmp);

    ConfigStatus_t st;
    st = flash_write_block(CONFIG_PAGE_PRIMARY, &tmp);
    if (st != CONFIG_OK) return st;
    st = flash_write_block(CONFIG_PAGE_BACKUP, &tmp);
    if (st != CONFIG_OK) return st;

    g_config = tmp;
    g_config_dirty = false;
    Config_Apply();
    return CONFIG_OK;
}

/* ===== Dirty flag helpers =========================================== */

void Config_MarkDirty(void) { g_config_dirty = true; }
bool Config_IsDirty(void)   { return g_config_dirty; }
