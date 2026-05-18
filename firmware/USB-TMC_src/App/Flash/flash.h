/*
 * flash.h — persistent configuration storage (3-tier: DEFAULT / PRIMARY / BACKUP)
 */

#ifndef APP_FLASH_FLASH_H_
#define APP_FLASH_FLASH_H_

#include <stdint.h>
#include <stdbool.h>

/* ===== Flash memory layout (FLASHEE region, 6 KB) ===================
 *
 *   Page 56  0x0801C000  DEFAULT  – factory defaults, const (firmware-baked)
 *   Page 57  0x0801C800  PRIMARY  – active config, runtime writable
 *   Page 58  0x0801D000  BACKUP   – copy of PRIMARY, runtime writable
 *
 * ===================================================================== */

#define CONFIG_ADDR_DEFAULT  0x0801C000UL
#define CONFIG_ADDR_PRIMARY  0x0801C800UL
#define CONFIG_ADDR_BACKUP   0x0801D000UL

#define CONFIG_PAGE_DEFAULT  56u
#define CONFIG_PAGE_PRIMARY  57u
#define CONFIG_PAGE_BACKUP   58u

#define CONFIG_MAGIC    0x5344544BUL   /* "SDTK" */
#define CONFIG_VERSION  2u             /* bumped: added calibration block */

/* ===== Configuration block =========================================== */

typedef struct __attribute__((packed)) {
    /* Header */
    uint32_t magic;               /* CONFIG_MAGIC */
    uint32_t version;             /* CONFIG_VERSION */

    /* Display */
    uint8_t  dispBrightness;      /* 1-100 % */
    uint8_t  dispState;           /* 0=OFF 1=ON */
    uint8_t  dispSource;          /* DisplaySource_t */
    uint8_t  _r0;

    /* SHT45 */
    uint8_t  sht45Precision;      /* SHT45_Precision_t raw (0xFD/0xF6/0xE0) */
    uint8_t  sht45HeaterMode;     /* SHT45_HeaterMode_t raw */
    uint16_t sht45ReadPeriodMs;   /* 50-60000 ms */
    uint8_t  sht45AverageCount;   /* 1-255 */
    uint8_t  _r1[3];

    /* TMP117 */
    uint8_t  tmp117Mode;          /* TMP117_Mode_t */
    uint8_t  tmp117ConvRate;      /* 0-7 */
    uint8_t  tmp117Avg;           /* TMP117_Averaging_t */
    uint8_t  _r2;
    uint16_t tmp117ReadPeriodMs;  /* firmware polling period ms */
    int16_t  tmp117AlertHighRaw;  /* THIGH shadow (raw, 1LSB=0.0078125°C) */
    int16_t  tmp117AlertLowRaw;   /* TLOW shadow */
    uint8_t  _r3[2];

    /* Polynomial calibration — T_cal = a0 + a1*T + a2*T^2 + a3*T^3
     * Identity coefficients (a0=0, a1=1, a2=0, a3=0) mean no correction. */
    float    cal_a0;              /* constant offset [°C] */
    float    cal_a1;              /* linear gain (1.0 = identity) */
    float    cal_a2;              /* quadratic coefficient */
    float    cal_a3;              /* cubic coefficient */
    uint8_t  cal_active;          /* 0=off, 1=apply calibration */
    uint8_t  _r4;
    char     cal_date[10];        /* "YYYY-MM-DD" (no null in flash) */
    uint8_t  _r5[4];

    /* Integrity — CRC32 over the preceding 64 bytes (16 × uint32_t) */
    uint32_t crc32;               /* 0 in DEFAULT block (not validated) */
    uint32_t _pad;                /* align to 8 bytes for DOUBLEWORD write */
} SDT_Config_t;

_Static_assert(sizeof(SDT_Config_t) == 72u, "SDT_Config_t size must be 72 bytes");

/* ===== Status ======================================================== */

typedef enum {
    CONFIG_OK        = 0,
    CONFIG_ERR_MAGIC = 1,
    CONFIG_ERR_CRC   = 2,
    CONFIG_ERR_FLASH = 3,
} ConfigStatus_t;

/* ===== Global state ================================================== */

extern SDT_Config_t g_config;   /* RAM working copy (current settings) */
extern bool         g_config_dirty; /* true when RAM differs from PRIMARY flash */

/* ===== API =========================================================== */

/* Load config from flash on boot; apply to hardware drivers */
void Config_Init(void);

/* Apply g_config to all hardware drivers (display, sensor) */
void Config_Apply(void);

/* Snapshot current driver state into g_config */
void Config_Capture(void);

/* Write g_config to PRIMARY + BACKUP (with CRC) */
ConfigStatus_t Config_Save(void);

/* Copy BACKUP → PRIMARY (repair corrupted PRIMARY) */
ConfigStatus_t Config_Restore(void);

/* Copy DEFAULT → PRIMARY + BACKUP (factory reset) */
ConfigStatus_t Config_Recall(void);

/* Mark RAM config as modified (unsaved) */
void Config_MarkDirty(void);

/* Query dirty flag */
bool Config_IsDirty(void);

#endif /* APP_FLASH_FLASH_H_ */
