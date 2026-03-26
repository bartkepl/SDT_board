/*
 * scpi_def.c
 *
 *  Created on: Dec 21, 2025
 *      Author: bartkepl
 */


#include "scpi-def.h"
#include "stm32c0xx_hal.h"
#include <string.h>
#include <usbtmc_app.h>
#include <display.h>
#include <sensor.h>
#include <Utils.h>


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


	{.pattern = "DISPlay:BRIGhtness?", .callback = SCPI_DisplayBrightnessQ,},			// Odczyt aktualnej jasności
	{.pattern = "DISPlay:BRIGhtness", .callback = SCPI_DisplayBrightness,},			// Ustawienie jasności wyświetlacza {1-100[%]}
	{.pattern = "DISPlay:STATe?", .callback = SCPI_DisplayStateQ,},					// Odczyt stanu wyświetlacza
	{.pattern = "DISPlay:STATe", .callback = SCPI_DisplayState,},					// Ustawienie stanu wyświetlacza {0,1,ON,OFF}
	{.pattern = "DISPlay:SOURce", .callback = SCPI_DisplaySource,},					// Ustawienie źródła danych wyświetlacza
	{.pattern = "DISPlay:SOURce?", .callback = SCPI_DisplaySourceQ,},					// Ustawienie źródła danych wyświetlacza
	{.pattern = "DISPlay:TEXT", .callback = SCPI_DisplayText,},						// Ustawienie textu na wyświetlacz
	{.pattern = "DISPlay:TEXT?", .callback = SCPI_DisplayTextQ,},						// Odczytanie textu na wyświetlaczu


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

/* ===== SCPI callbacks ===== */

static inline size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
	(void) context;
	setReply(data, len);
	return len;
}

static inline int SCPI_Error(scpi_t *context, int_fast16_t err) {
	(void) context;
	(void) err;
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

	SCPI_ResultInt32(context, 0);

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorTypeQ(scpi_t *context) {

	switch (g_sensor.type) {
	case SENSOR_SHT45:
		SCPI_ResultText(context, "SHT45");
		break;
	case SENSOR_TMP117:
		SCPI_ResultText(context, "TMP117");
		break;
	default:
		SCPI_ResultText(context, "UNKNWN");
		break;
	}
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorTemperatureQ(scpi_t *context) {
	if (!g_sensor.ucValidFlag)
		return SCPI_RES_ERR;

	SCPI_ResultFloat(context, g_sensor.fTemp);

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorIdQ(scpi_t *context) {
	if (!g_sensor.ucValidFlag)
		return SCPI_RES_ERR;

	SCPI_ResultInt32(context, (uint32_t) g_sensor.usSensorId);

	return SCPI_RES_OK;
}
static scpi_result_t SCPI_SensorHumidityQ(scpi_t *context) {
	if (!g_sensor.ucValidFlag)
		return SCPI_RES_ERR;

	if (g_sensor.type == SENSOR_SHT45) {
		SCPI_ResultFloat(context, g_sensor.fHum);
	} else {
		SCPI_ResultText(context, "NOT_SUPPORTED");
	}

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorHeater(scpi_t *context) {
	if (!g_sensor.ucValidFlag)
		return SCPI_RES_ERR;

	Sensor_SHT45Heater();

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
	uint32_t brightness = 101;
	if (!SCPI_ParamUInt32(context, &brightness, 1))
		return SCPI_RES_ERR;

	if (brightness <= 100) {
		Display_SetBrightness(brightness);
	} else {
		return SCPI_RES_ERR;
	}

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayStateQ(scpi_t *context) {
	SCPI_ResultInt32(context, Display_GetState());
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayState(scpi_t *context) {
	bool state = 1;
	if (!SCPI_ParamBool(context, &state, 1))
		return SCPI_RES_ERR;

	Display_SetState(state);
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplaySource(scpi_t *context) {
	uint32_t source = 0;
	if (!SCPI_ParamUInt32(context, &source, 1))
		return SCPI_RES_ERR;

	if (source >= eDisplaySource_SIZE)
		return SCPI_RES_ERR;

	Display_SelectSource((DisplaySource_t) source);
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
		return SCPI_RES_ERR;

	char buf[8];

	memset(buf, ' ', 8);
	memcpy(buf, ptr, len > 8 ? 8 : len);

	Display_SetText(buf);

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayTextQ(scpi_t *context) {

	SCPI_ResultCharacters(context, Display_GetText(), 8);

	return SCPI_RES_OK;
}

