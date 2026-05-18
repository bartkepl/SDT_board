/*
 * scpi_def.c
 *
 *  Created on: Dec 21, 2025
 *      Author: bartkepl
 */


#include "scpi-def.h"
#include "stm32c0xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <usbtmc_app.h>
#include <display.h>
#include <sensor.h>
#include <sht45.h>
#include <tmp117.h>
#include <Utils.h>
#include <flash.h>


/* Push error code and return SCPI_RES_ERR in one step */
#define SCPI_PUSH_ERR(ctx, code)  do { SCPI_ErrorPush((ctx), (code)); return SCPI_RES_ERR; } while(0)

/* ===== SCPI callbacks ===== */

static size_t SCPI_Write(scpi_t *context, const char *data, size_t len);
static int SCPI_Error(scpi_t *context, int_fast16_t err);
static scpi_result_t SCPI_Reset(scpi_t *context);

/* ===== SCPI command handlers ===== */

static scpi_result_t My_CoreTstQ(scpi_t * context);

static scpi_result_t SCPI_SensorTypeQ(scpi_t *context);
static scpi_result_t SCPI_SensorTemperatureQ(scpi_t *context);
static scpi_result_t SCPI_SensorIdQ(scpi_t *context);
static scpi_result_t SCPI_SensorHumidityQ(scpi_t *context);
static scpi_result_t SCPI_SensorHeater(scpi_t *context);
static scpi_result_t SCPI_SensorReadPeriodQ(scpi_t *context);
static scpi_result_t SCPI_SensorReadPeriod(scpi_t *context);
static scpi_result_t SCPI_SensorAverageQ(scpi_t *context);
static scpi_result_t SCPI_SensorAverage(scpi_t *context);
static scpi_result_t SCPI_SensorPrecisionQ(scpi_t *context);
static scpi_result_t SCPI_SensorPrecision(scpi_t *context);
static scpi_result_t SCPI_SystemBootloaderEnter(scpi_t *context);
static scpi_result_t SCPI_SystemReset(scpi_t *context);
static scpi_result_t SCPI_SystemIdQ(scpi_t *context);
static scpi_result_t SCPI_DisplayBrightnessQ(scpi_t *context);
static scpi_result_t SCPI_DisplayBrightness(scpi_t *context);
static scpi_result_t SCPI_DisplayStateQ(scpi_t *context);
static scpi_result_t SCPI_DisplayState(scpi_t *context);
static scpi_result_t SCPI_DisplaySource(scpi_t *context);
static scpi_result_t SCPI_DisplaySourceQ(scpi_t *context);
static scpi_result_t SCPI_DisplayText(scpi_t *context);
static scpi_result_t SCPI_DisplayTextQ(scpi_t *context);

// TMP117-specific and generalized handlers
static scpi_result_t SCPI_SensorAlertHighQ(scpi_t *context);
static scpi_result_t SCPI_SensorAlertHigh(scpi_t *context);
static scpi_result_t SCPI_SensorAlertLowQ(scpi_t *context);
static scpi_result_t SCPI_SensorAlertLow(scpi_t *context);
static scpi_result_t SCPI_SensorAlertStatusQ(scpi_t *context);
static scpi_result_t SCPI_SensorModeQ(scpi_t *context);
static scpi_result_t SCPI_SensorMode(scpi_t *context);
static scpi_result_t SCPI_SensorConvRateQ(scpi_t *context);
static scpi_result_t SCPI_SensorConvRate(scpi_t *context);
static scpi_result_t SCPI_SensorSoftReset(scpi_t *context);

/* Config storage commands */
static scpi_result_t SCPI_ConfigSave(scpi_t *context);
static scpi_result_t SCPI_ConfigRestore(scpi_t *context);
static scpi_result_t SCPI_ConfigRecall(scpi_t *context);
static scpi_result_t SCPI_ConfigDirtyQ(scpi_t *context);

/* Calibration commands */
static scpi_result_t SCPI_CalCoeff(scpi_t *context);
static scpi_result_t SCPI_CalCoeffQ(scpi_t *context);
static scpi_result_t SCPI_CalState(scpi_t *context);
static scpi_result_t SCPI_CalStateQ(scpi_t *context);
static scpi_result_t SCPI_CalDate(scpi_t *context);
static scpi_result_t SCPI_CalDateQ(scpi_t *context);
static scpi_result_t SCPI_CalReset(scpi_t *context);

/* ===== SCPI command list ===== */

static const scpi_command_t scpi_commands[] = {
	/* IEEE Mandated Commands (SCPI std V1999.0 4.1.1) */
	{ .pattern = "*CLS", .callback = SCPI_CoreCls,},
	{ .pattern = "*ESE", .callback = SCPI_CoreEse,},
	{ .pattern = "*ESE?", .callback = SCPI_CoreEseQ,},
	{ .pattern = "*ESR?", .callback = SCPI_CoreEsrQ,},
    { .pattern = "*IDN?", .callback = SCPI_CoreIdnQ,},
	{ .pattern = "*OPC", .callback = SCPI_CoreOpc,},
	{ .pattern = "*OPC?", .callback = SCPI_CoreOpcQ,},
	{ .pattern = "*RST", .callback = SCPI_CoreRst,},
	{ .pattern = "*SRE", .callback = SCPI_CoreSre,},
	{ .pattern = "*SRE?", .callback = SCPI_CoreSreQ,},
	{ .pattern = "*STB?", .callback = SCPI_CoreStbQ,},
	{ .pattern = "*TST?", .callback = My_CoreTstQ,},
	{ .pattern = "*WAI", .callback = SCPI_CoreWai,},

	/* Required SCPI commands (SCPI std V1999.0 4.2.1) */
	{.pattern = "SYSTem:ERRor[:NEXT]?", .callback = SCPI_SystemErrorNextQ,},
	{.pattern = "SYSTem:ERRor:COUNt?", .callback = SCPI_SystemErrorCountQ,},
	{.pattern = "SYSTem:VERSion?", .callback = SCPI_SystemVersionQ,},


	/* Custom SCPI commands */
	{.pattern = "SYSTem:BOOTloader:ENter", .callback = SCPI_SystemBootloaderEnter,},		// Restart do trybu USB DFU {-}
	{.pattern = "SYSTem:RST", .callback = SCPI_SystemReset,},								// Restart {-}
	{.pattern = "SYSTem:ID?", .callback = SCPI_SystemIdQ,},									// Zwraca ID STM32 {LONG(20b) lub SHORT(4b)}


	{.pattern = "SENSor:TYPE?", .callback = SCPI_SensorTypeQ,},						// odczyt typu czujnika ("SHT45" / "TMP117")
	{.pattern = "SENSor:TEMPerature?", .callback = SCPI_SensorTemperatureQ,},		// odczyt zmierzonej temperatury (float)
	{.pattern = "SENSor:ID?", .callback = SCPI_SensorIdQ,},							// odczyt ID czujnika (uint32_t)
	{.pattern = "SENSor:HUMidity?", .callback = SCPI_SensorHumidityQ,},				// odczyt zmierzonej wilgotności (float tylko dla SHT45)
	{.pattern = "SENSor:HEATer", .callback = SCPI_SensorHeater,},					// uruchomienie grzałki wbudowanej w SHT45 (tylko dla SHT45)
	{.pattern = "SENSor:READperiod?", .callback = SCPI_SensorReadPeriodQ,},		// odczyt okresu pomiarów [ms] (SHT45 only)
	{.pattern = "SENSor:READperiod", .callback = SCPI_SensorReadPeriod,},			// ustawienie okresu pomiarów [ms] (SHT45 only)
	{.pattern = "SENSor:AVErage?", .callback = SCPI_SensorAverageQ,},				// odczyt liczby pomiarów do uśredniania
	{.pattern = "SENSor:AVErage", .callback = SCPI_SensorAverage,},					// ustawienie liczby pomiarów do uśredniania (1-255)
	{.pattern = "SENSor:PRECision?", .callback = SCPI_SensorPrecisionQ,},			// odczyt dokładności (0=LOW, 1=MEDIUM, 2=HIGH) (SHT45 only)
	{.pattern = "SENSor:PRECision", .callback = SCPI_SensorPrecision,},				// ustawienie dokładności (0=LOW, 1=MEDIUM, 2=HIGH) (SHT45 only)


	{.pattern = "DISPlay:BRIGhtness?", .callback = SCPI_DisplayBrightnessQ,},			// Odczyt aktualnej jasności
	{.pattern = "DISPlay:BRIGhtness", .callback = SCPI_DisplayBrightness,},			// Ustawienie jasności wyświetlacza {1-100[%]}
	{.pattern = "DISPlay:STATe?", .callback = SCPI_DisplayStateQ,},					// Odczyt stanu wyświetlacza
	{.pattern = "DISPlay:STATe", .callback = SCPI_DisplayState,},					// Ustawienie stanu wyświetlacza {0,1,ON,OFF}
	{.pattern = "DISPlay:SOURce", .callback = SCPI_DisplaySource,},					// Ustawienie źródła danych wyświetlacza
	{.pattern = "DISPlay:SOURce?", .callback = SCPI_DisplaySourceQ,},					// Ustawienie źródła danych wyświetlacza
	{.pattern = "DISPlay:TEXT", .callback = SCPI_DisplayText,},						// Ustawienie textu na wyświetlacz
	{.pattern = "DISPlay:TEXT?", .callback = SCPI_DisplayTextQ,},						// Odczytanie textu na wyświetlaczu

	// TMP117 alert thresholds (°C)
	{.pattern = "SENSor:ALERt:HIGH?",   .callback = SCPI_SensorAlertHighQ,  },
	{.pattern = "SENSor:ALERt:HIGH",    .callback = SCPI_SensorAlertHigh,   },
	{.pattern = "SENSor:ALERt:LOW?",    .callback = SCPI_SensorAlertLowQ,   },
	{.pattern = "SENSor:ALERt:LOW",     .callback = SCPI_SensorAlertLow,    },
	{.pattern = "SENSor:ALERt:STATus?", .callback = SCPI_SensorAlertStatusQ,},

	// TMP117 conversion mode and rate
	{.pattern = "SENSor:MODe?",         .callback = SCPI_SensorModeQ,       },
	{.pattern = "SENSor:MODe",          .callback = SCPI_SensorMode,        },
	{.pattern = "SENSor:CONVrate?",     .callback = SCPI_SensorConvRateQ,   },
	{.pattern = "SENSor:CONVrate",      .callback = SCPI_SensorConvRate,    },

	// Soft reset (both sensors)
	{.pattern = "SENSor:SOFTReset",     .callback = SCPI_SensorSoftReset,   },

	// Configuration storage
	{.pattern = "SYSTem:CONFig:SAVE",    .callback = SCPI_ConfigSave,    },
	{.pattern = "SYSTem:CONFig:RESTore", .callback = SCPI_ConfigRestore, },
	{.pattern = "SYSTem:CONFig:RECall",  .callback = SCPI_ConfigRecall,  },
	{.pattern = "SYSTem:CONFig:DIRty?",  .callback = SCPI_ConfigDirtyQ,  },

	// Polynomial calibration
	{.pattern = "CALibration:COEFficient",   .callback = SCPI_CalCoeff,   },
	{.pattern = "CALibration:COEFficient?",  .callback = SCPI_CalCoeffQ,  },
	{.pattern = "CALibration:STATe",         .callback = SCPI_CalState,   },
	{.pattern = "CALibration:STATe?",        .callback = SCPI_CalStateQ,  },
	{.pattern = "CALibration:DATE",          .callback = SCPI_CalDate,    },
	{.pattern = "CALibration:DATE?",         .callback = SCPI_CalDateQ,   },
	{.pattern = "CALibration:RESet",         .callback = SCPI_CalReset,   },

    SCPI_CMD_LIST_END
};

/* ===== SCPI interface ===== */

static scpi_interface_t scpi_interface = {
    .write = SCPI_Write,
    .error = SCPI_Error,
    .reset = SCPI_Reset,
};

/* ===== SCPI context ===== */

scpi_t scpi_context;

static char scpi_input_buffer[1024];
static scpi_error_t scpi_error_queue[16];

/* ===== Public API ===== */

void SCPI_Main_Init(void)
{
    SCPI_Init(
        &scpi_context,              /* scpi_t * context */
        scpi_commands,              /* command list */
        &scpi_interface,            /* interface */
        NULL,                       /* units (nie używamy) */
		SCPI_IDN_MANUFACTURER ,     /* *IDN? manufacturer */
		SCPI_IDN_MODEL,             /* *IDN? model */
		serial_get(),              /* *IDN? serial number */
		SCPI_IDN_FW,                /* *IDN? firmware version */
        scpi_input_buffer,          /* input buffer */
        sizeof(scpi_input_buffer),  /* buffer length */
        scpi_error_queue,           /* error queue */
        16                          /* error queue size */
    );
}

void SCPI_Main_Input(const char *data, uint32_t len) {
	SCPI_Input(&scpi_context, data, len);
}

void SCPI_Main_Poll(void) {
	switch (Sensor_GetAndClearError()) {
		case SENSOR_ERR_NOT_FOUND: SCPI_ErrorPush(&scpi_context, SCPI_ERROR_HARDWARE_MISSING); break;
		case SENSOR_ERR_COMM:      SCPI_ErrorPush(&scpi_context, SCPI_ERROR_HARDWARE_ERROR);   break;
		case SENSOR_ERR_TIMEOUT:   SCPI_ErrorPush(&scpi_context, SCPI_ERROR_TIME_OUT);         break;
		case SENSOR_ERR_DATA:      SCPI_ErrorPush(&scpi_context, SCPI_ERROR_DATA_CORRUPT);     break;
		default: break;
	}
}

/* ===== SCPI callbacks ===== */

static inline size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
	(void) context;
	setReply(data, len);
	return len;
}

static int SCPI_Error(scpi_t *context, int_fast16_t err) {
	(void) context;
	if (err != 0)
		Display_ShowError((int16_t)err);
	return 0;
}

static inline scpi_result_t SCPI_Reset(scpi_t *context) {
	(void) context;
	return SCPI_RES_OK;
}

/**
 * Reimplement IEEE488.2 *TST?
 *
 * Result should be 0 if everything is ok
 * Result should be 1 if something goes wrong
 *
 * Return SCPI_RES_OK
 */
static scpi_result_t My_CoreTstQ(scpi_t *context) {
	int32_t result = 0;

	if (g_sensor.type == SENSOR_NONE || g_sensor.type == SENSOR_ERROR) {
		result |= 1;
		SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
	}

	SCPI_ResultInt32(context, result);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorTypeQ(scpi_t *context) {

	switch (g_sensor.type) {
	case SENSOR_SHT45:  SCPI_ResultText(context, "SHT45");  break;
	case SENSOR_TMP117: SCPI_ResultText(context, "TMP117"); break;
	case SENSOR_DUAL:   SCPI_ResultText(context, "DUAL");   break;
	default:            SCPI_ResultText(context, "UNKNWN"); break;
	}
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorTemperatureQ(scpi_t *context) {
	if (g_sensor.type == SENSOR_ERROR)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);
	if (g_sensor.type == SENSOR_NONE)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);
	if (!g_sensor.ucValidFlag)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_QUESTIONABLE);

	SCPI_ResultFloat(context, g_sensor.fTemp);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorIdQ(scpi_t *context) {
	if (g_sensor.type == SENSOR_ERROR)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);
	if (g_sensor.type == SENSOR_NONE)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);
	if (!g_sensor.ucValidFlag)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_QUESTIONABLE);

	SCPI_ResultInt32(context, (uint32_t) g_sensor.usSensorId);
	return SCPI_RES_OK;
}
static scpi_result_t SCPI_SensorHumidityQ(scpi_t *context) {
	if (g_sensor.type == SENSOR_ERROR)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);
	if (g_sensor.type == SENSOR_NONE)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);
	if (!g_sensor.ucValidFlag)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_QUESTIONABLE);

	if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL) {
		SCPI_ResultFloat(context, g_sensor.fHum);
	} else {
		/* TMP117 has no humidity sensor — return SCPI NaN */
		SCPI_ResultFloat(context, (float)NAN);
	}
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorHeater(scpi_t *context) {
	if (g_sensor.type == SENSOR_TMP117)
		SCPI_PUSH_ERR(context, SCPI_ERROR_SETTINGS_CONFLICT);
	if (!g_sensor.ucValidFlag)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_QUESTIONABLE);

	// Request heater with default power level (20mW, 1s)
	Sensor_SHT45_RequestHeater(0x1E);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SystemBootloaderEnter(scpi_t *context) {

	DisplayClearAll();
	DisplayOff();

#define BOOT_ADDR	0x1FFF0000	// my MCU boot code base address
#define	MCU_IRQS	48u	// no. of NVIC IRQ inputs

	struct boot_vectable_ {
		uint32_t Initial_SP;
		void (*Reset_Handler)(void);
	};

#define BOOTVTAB	((struct boot_vectable_ *)BOOT_ADDR)

	/* Disable all interrupts */
	__disable_irq();

	/* Disable Systick timer */
	SysTick->CTRL = 0;

	/* Set the clock to the default state */
	HAL_RCC_DeInit();

	/* Clear Interrupt Enable Register & Interrupt Pending Register */
	for (uint8_t i = 0; i < (MCU_IRQS + 31u) / 32; i++) {
		NVIC->ICER[i] = 0xFFFFFFFF;
		NVIC->ICPR[i] = 0xFFFFFFFF;
	}

	/* Re-enable all interrupts */
	__enable_irq();

	// Set the MSP
	__set_MSP(BOOTVTAB->Initial_SP);

	// Jump to app firmware
	BOOTVTAB->Reset_Handler();

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SystemReset(scpi_t *context) {
	HAL_NVIC_SystemReset();
	return SCPI_RES_OK;
}

typedef enum {
	SERIAL_OPT_SHORT = 0, SERIAL_OPT_LONG,
} serial_opt_t;

static const scpi_choice_def_t serial_options[] = {
		{ "SHORT", SERIAL_OPT_SHORT }, { "LONG", SERIAL_OPT_LONG },
		SCPI_CHOICE_LIST_END };

static scpi_result_t SCPI_SystemIdQ(scpi_t *context) {

	int32_t opt = SERIAL_OPT_SHORT;

	// 🔹 parametr opcjonalny → FALSE
	if (!SCPI_ParamChoice(context, serial_options, &opt, FALSE)) {
		opt = SERIAL_OPT_SHORT;
	}

	switch (opt) {
	case SERIAL_OPT_LONG:
		SCPI_ResultCharacters(context, serial_get_full(), 40);
		break;

	case SERIAL_OPT_SHORT:
	default:
		SCPI_ResultCharacters(context, serial_get(), 8);
		break;
	}

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayBrightnessQ(scpi_t *context) {
	SCPI_ResultInt32(context, Display_GetBrightness());
	return SCPI_RES_OK;
}
static scpi_result_t SCPI_DisplayBrightness(scpi_t *context) {
	uint32_t brightness = 0;
	if (!SCPI_ParamUInt32(context, &brightness, 1))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);

	if (brightness > 100)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);

	Display_SetBrightness(brightness);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayStateQ(scpi_t *context) {
	SCPI_ResultInt32(context, Display_GetState());
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayState(scpi_t *context) {
	bool state = 1;
	if (!SCPI_ParamBool(context, &state, 1))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);

	Display_SetState(state);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplaySource(scpi_t *context) {
	uint32_t source = 0;
	if (!SCPI_ParamUInt32(context, &source, 1))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);

	if (source >= eDisplaySource_SIZE)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);

	Display_SelectSource((DisplaySource_t) source);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplaySourceQ(scpi_t *context) {
	SCPI_ResultInt32(context, Display_GetSource());
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayText(scpi_t *context) {
	const char *ptr;
	size_t len;

	if (!SCPI_ParamCharacters(context, &ptr, &len, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);

	char buf[8];

	memset(buf, ' ', 8);
	memcpy(buf, ptr, len > 8 ? 8 : len);

	Display_SetText(buf, 8);

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayTextQ(scpi_t *context) {

	SCPI_ResultCharacters(context, Display_GetText(), 8);

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorReadPeriodQ(scpi_t *context) {
	switch (g_sensor.type) {
	case SENSOR_SHT45:
	case SENSOR_DUAL:
		SCPI_ResultInt32(context, Sensor_SHT45_GetReadPeriod());
		break;
	case SENSOR_TMP117:
		SCPI_ResultInt32(context, Sensor_TMP117_GetReadPeriod());
		break;
	default:
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);
	}
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorReadPeriod(scpi_t *context) {
	if (g_sensor.type == SENSOR_NONE)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);
	if (g_sensor.type == SENSOR_ERROR)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);

	uint32_t periodMs = 500;
	if (!SCPI_ParamUInt32(context, &periodMs, 1))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);

	if (periodMs < 50 || periodMs > 60000)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);

	if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
		Sensor_SHT45_SetReadPeriod((uint16_t)periodMs);
	if (g_sensor.type == SENSOR_TMP117)
		Sensor_TMP117_SetReadPeriod((uint16_t)periodMs);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorAverageQ(scpi_t *context) {
	static const uint8_t tmp117AvgMap[] = {1, 8, 32, 64};

	switch (g_sensor.type) {
	case SENSOR_SHT45:
	case SENSOR_DUAL:
		SCPI_ResultInt32(context, Sensor_SHT45_GetAverageCount());
		break;
	case SENSOR_TMP117:
	{
		uint8_t avg = Sensor_TMP117_GetAvgHW();
		SCPI_ResultInt32(context, tmp117AvgMap[avg < 4u ? avg : 0u]);
		break;
	}
	default:
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);
	}
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorAverage(scpi_t *context) {
	if (g_sensor.type == SENSOR_NONE)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);
	if (g_sensor.type == SENSOR_ERROR)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);

	uint32_t count = 1;
	if (!SCPI_ParamUInt32(context, &count, 1))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);

	if (g_sensor.type == SENSOR_SHT45 || g_sensor.type == SENSOR_DUAL)
	{
		if (count < 1 || count > 255)
			SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
		Sensor_SHT45_SetAverageCount((uint8_t)count);
	}
	else if (g_sensor.type == SENSOR_TMP117)
	{
		/* TMP117 hardware averaging: only 1, 8, 32, 64 are valid */
		TMP117_Averaging_t avg;
		if      (count == 1u)  avg = TMP117_AVG_1;
		else if (count == 8u)  avg = TMP117_AVG_8;
		else if (count == 32u) avg = TMP117_AVG_32;
		else if (count == 64u) avg = TMP117_AVG_64;
		else SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
		Sensor_TMP117_SetAvgHW((uint8_t)avg);
	}
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static const scpi_choice_def_t precision_options[] = {
	{ "LOW", SHT45_PRECISION_LOW },
	{ "MEDIUM", SHT45_PRECISION_MEDIUM },
	{ "HIGH", SHT45_PRECISION_HIGH },
	SCPI_CHOICE_LIST_END
};

static scpi_result_t SCPI_SensorPrecisionQ(scpi_t *context) {
	if (g_sensor.type == SENSOR_TMP117) {
		/* TMP117 has no software precision selection */
		SCPI_ResultFloat(context, (float)NAN);
		return SCPI_RES_OK;
	}
	if (g_sensor.type != SENSOR_SHT45 && g_sensor.type != SENSOR_DUAL)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);

	switch ((SHT45_Precision_t)Sensor_SHT45_GetMeasurementPrecision()) {
		case SHT45_PRECISION_LOW:    SCPI_ResultText(context, "LOW");    break;
		case SHT45_PRECISION_MEDIUM: SCPI_ResultText(context, "MEDIUM"); break;
		case SHT45_PRECISION_HIGH:   SCPI_ResultText(context, "HIGH");   break;
		default: SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);
	}
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorPrecision(scpi_t *context) {
	if (g_sensor.type == SENSOR_TMP117)
		SCPI_PUSH_ERR(context, SCPI_ERROR_SETTINGS_CONFLICT);
	if (g_sensor.type != SENSOR_SHT45 && g_sensor.type != SENSOR_DUAL)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);

	int32_t precision = SHT45_PRECISION_HIGH;
	if (!SCPI_ParamChoice(context, precision_options, &precision, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);

	Sensor_SHT45_SetMeasurementPrecision((uint8_t)precision);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

//------------------------------------------------------------------//
// TMP117-specific and generalized handlers
//------------------------------------------------------------------//

static const scpi_choice_def_t mode_options[] = {
	{ "CONTinuous", TMP117_MODE_CONTINUOUS },
	{ "SHUTdown",   TMP117_MODE_SHUTDOWN   },
	{ "ONESHot",    TMP117_MODE_ONESHOT    },
	SCPI_CHOICE_LIST_END
};

#define TMP117_GUARD() \
	do { if (g_sensor.type != SENSOR_TMP117 && g_sensor.type != SENSOR_DUAL) \
		SCPI_PUSH_ERR(context, SCPI_ERROR_SETTINGS_CONFLICT); } while(0)

static scpi_result_t SCPI_SensorAlertHighQ(scpi_t *context) {
	TMP117_GUARD();
	SCPI_ResultFloat(context, Sensor_TMP117_GetAlertHigh());
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorAlertHigh(scpi_t *context) {
	TMP117_GUARD();
	float threshold = 0.0f;
	if (!SCPI_ParamFloat(context, &threshold, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	/* Operational range -55°C to +150°C */
	if (threshold < -55.0f || threshold > 150.0f)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
	Sensor_TMP117_SetAlertHigh(threshold);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorAlertLowQ(scpi_t *context) {
	TMP117_GUARD();
	SCPI_ResultFloat(context, Sensor_TMP117_GetAlertLow());
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorAlertLow(scpi_t *context) {
	TMP117_GUARD();
	float threshold = 0.0f;
	if (!SCPI_ParamFloat(context, &threshold, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	if (threshold < -55.0f || threshold > 150.0f)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
	Sensor_TMP117_SetAlertLow(threshold);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorAlertStatusQ(scpi_t *context) {
	TMP117_GUARD();
	uint8_t status = Sensor_TMP117_GetAlertStatus();
	if      (status & 0x02u) SCPI_ResultText(context, "HIGH");
	else if (status & 0x01u) SCPI_ResultText(context, "LOW");
	else if (status & 0x04u) SCPI_ResultText(context, "READY");
	else                     SCPI_ResultText(context, "NONE");
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorModeQ(scpi_t *context) {
	TMP117_GUARD();
	switch ((TMP117_Mode_t)Sensor_TMP117_GetMode()) {
	case TMP117_MODE_CONTINUOUS: SCPI_ResultText(context, "CONTINUOUS"); break;
	case TMP117_MODE_SHUTDOWN:   SCPI_ResultText(context, "SHUTDOWN");   break;
	case TMP117_MODE_ONESHOT:    SCPI_ResultText(context, "ONESHOT");    break;
	default: return SCPI_RES_ERR;
	}
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorMode(scpi_t *context) {
	TMP117_GUARD();
	int32_t mode = TMP117_MODE_CONTINUOUS;
	if (!SCPI_ParamChoice(context, mode_options, &mode, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	Sensor_TMP117_SetMode((uint8_t)mode);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorConvRateQ(scpi_t *context) {
	TMP117_GUARD();
	SCPI_ResultInt32(context, Sensor_TMP117_GetConvRate());
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorConvRate(scpi_t *context) {
	TMP117_GUARD();
	uint32_t rate = 4;
	if (!SCPI_ParamUInt32(context, &rate, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	if (rate > 7u)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
	Sensor_TMP117_SetConvRate((uint8_t)rate);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorSoftReset(scpi_t *context) {
	switch (g_sensor.type) {
	case SENSOR_TMP117:
		Sensor_TMP117_RequestSoftReset();
		break;
	case SENSOR_SHT45:
		Sensor_SHT45_RequestSoftReset();
		break;
	case SENSOR_DUAL:
		Sensor_TMP117_RequestSoftReset();
		Sensor_SHT45_RequestSoftReset();
		break;
	default:
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_MISSING);
	}
	return SCPI_RES_OK;
}

//------------------------------------------------------------------//
// Configuration storage handlers
//------------------------------------------------------------------//

static scpi_result_t SCPI_ConfigSave(scpi_t *context) {
	if (Config_Save() != CONFIG_OK)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_ConfigRestore(scpi_t *context) {
	ConfigStatus_t st = Config_Restore();
	if (st == CONFIG_ERR_CRC || st == CONFIG_ERR_MAGIC)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_CORRUPT);
	if (st == CONFIG_ERR_FLASH)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_ConfigRecall(scpi_t *context) {
	if (Config_Recall() != CONFIG_OK)
		SCPI_PUSH_ERR(context, SCPI_ERROR_HARDWARE_ERROR);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_ConfigDirtyQ(scpi_t *context) {
	SCPI_ResultBool(context, Config_IsDirty());
	return SCPI_RES_OK;
}

//------------------------------------------------------------------//
// Polynomial calibration handlers
// T_cal = a0 + a1*T + a2*T^2 + a3*T^3
//------------------------------------------------------------------//

static scpi_result_t SCPI_CalCoeff(scpi_t *context) {
	double a0, a1, a2, a3;
	if (!SCPI_ParamDouble(context, &a0, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	if (!SCPI_ParamDouble(context, &a1, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	if (!SCPI_ParamDouble(context, &a2, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	if (!SCPI_ParamDouble(context, &a3, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);

	g_config.cal_a0 = (float)a0;
	g_config.cal_a1 = (float)a1;
	g_config.cal_a2 = (float)a2;
	g_config.cal_a3 = (float)a3;
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_CalCoeffQ(scpi_t *context) {
	char buf[64];
	int len = snprintf(buf, sizeof(buf), "%e,%e,%e,%e",
	    (double)g_config.cal_a0, (double)g_config.cal_a1,
	    (double)g_config.cal_a2, (double)g_config.cal_a3);
	SCPI_ResultCharacters(context, buf, (size_t)len);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_CalState(scpi_t *context) {
	bool state;
	if (!SCPI_ParamBool(context, &state, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	g_config.cal_active = state ? 1u : 0u;
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_CalStateQ(scpi_t *context) {
	SCPI_ResultBool(context, g_config.cal_active != 0u);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_CalDate(scpi_t *context) {
	const char *ptr;
	size_t len;
	if (!SCPI_ParamCharacters(context, &ptr, &len, TRUE))
		SCPI_PUSH_ERR(context, SCPI_ERROR_MISSING_PARAMETER);
	/* Expect exactly "YYYY-MM-DD" (10 chars) */
	if (len != 10)
		SCPI_PUSH_ERR(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
	memcpy(g_config.cal_date, ptr, 10u);
	Config_MarkDirty();
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_CalDateQ(scpi_t *context) {
	SCPI_ResultCharacters(context, g_config.cal_date, 10u);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_CalReset(scpi_t *context) {
	(void)context;
	g_config.cal_a0 = 0.0f;
	g_config.cal_a1 = 1.0f;
	g_config.cal_a2 = 0.0f;
	g_config.cal_a3 = 0.0f;
	g_config.cal_active = 0u;
	memcpy(g_config.cal_date, "----------", 10u);
	Config_MarkDirty();
	return SCPI_RES_OK;
}
