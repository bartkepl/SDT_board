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
static scpi_result_t SCPI_DisplayBrightnessQ(scpi_t *context);
static scpi_result_t SCPI_DisplayBrightness(scpi_t *context);
static scpi_result_t SCPI_DisplayStateQ(scpi_t *context);
static scpi_result_t SCPI_DisplayState(scpi_t *context);


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

	{.pattern = "SENSor:TYPE?", .callback = SCPI_SensorTypeQ,},						// odczyt typu czujnika ("SHT45" / "TMP117")
	{.pattern = "SENSor:TEMPerature?", .callback = SCPI_SensorTemperatureQ,},		// odczyt zmierzonej temperatury (float)
	{.pattern = "SENSor:ID?", .callback = SCPI_SensorIdQ,},							// odczyt ID czujnika (uint32_t)
	{.pattern = "SENSor:HUMidity?", .callback = SCPI_SensorHumidityQ,},				// odczyt zmierzonej wilgotności (float tylko dla SHT45)
	{.pattern = "SENSor:HEATer", .callback = SCPI_SensorHeater,},					// uruchomienie grzałki wbudowanej w SHT45 (tylko dla SHT45)


	//{.pattern = "DISPlay:BRIGhtness?", .callback = SCPI_DispalyBrightnessQ,},			// Odczyt aktualnej jasności
	{.pattern = "DISPlay:BRIGhtness", .callback = SCPI_DisplayBrightness,},			// Ustawienie jasności wyświetlacza {1-100[%]}
	//{.pattern = "DISPlay:STATe?", .callback = SCPI_DisplayStateQ,},					// Odczyt stanu wyświetlacza
	{.pattern = "DISPlay:STATe", .callback = SCPI_DisplayState,},					// Ustawienie stanu wyświetlacza {0,1,ON,OFF}

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
		SCPI_IDN_SERIAL,            /* *IDN? serial number */
		SCPI_IDN_FW,                /* *IDN? firmware version */
        scpi_input_buffer,          /* input buffer */
        sizeof(scpi_input_buffer),  /* buffer length */
        scpi_error_queue,           /* error queue */
        16                          /* error queue size */
    );
}


void SCPI_Main_Input(const char *data, uint32_t len)
{
    SCPI_Input(&scpi_context, data, len);
}


/* ===== SCPI callbacks ===== */

static size_t SCPI_Write(scpi_t *context, const char *data, size_t len)
{
    (void)context;
    setReply(data, len);
    return len;
}

static int SCPI_Error(scpi_t *context, int_fast16_t err)
{
    (void)context;
    (void)err;
    return 0;
}

static scpi_result_t SCPI_Reset(scpi_t *context)
{
    (void)context;
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
static scpi_result_t My_CoreTstQ(scpi_t * context) {

    SCPI_ResultInt32(context, 0);

    return SCPI_RES_OK;
}


static scpi_result_t SCPI_SensorTypeQ(scpi_t *context){

	switch(g_sensor.type)
	{
		case SENSOR_SHT45:
			SCPI_ResultText(context, "SHT45");
			break;
		case SENSOR_TMP117:
			SCPI_ResultText(context, "SHT45");
			break;
		default:
			SCPI_ResultText(context, "UNKNWN");
			break;
	}
    return SCPI_RES_OK;
}


static scpi_result_t SCPI_SensorTemperatureQ(scpi_t *context){
	if(g_sensor.valid != true) return SCPI_RES_ERR;

	SCPI_ResultFloat(context, g_sensor.temperature);

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorIdQ(scpi_t *context){
	if(g_sensor.valid != true) return SCPI_RES_ERR;

	SCPI_ResultInt32(context, (uint32_t)g_sensor.id);

	return SCPI_RES_OK;
}
static scpi_result_t SCPI_SensorHumidityQ(scpi_t *context){
	if(g_sensor.valid != true) return SCPI_RES_ERR;

	if(g_sensor.type == SENSOR_SHT45){
		SCPI_ResultFloat(context, g_sensor.humidity);
	} else {
		SCPI_ResultText(context, "NOT_SUPPORTED");
	}


	return SCPI_RES_OK;
}

static scpi_result_t SCPI_SensorHeater(scpi_t *context){
	if(g_sensor.valid != true) return SCPI_RES_ERR;

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

static scpi_result_t SCPI_SystemReset(scpi_t *context)
{
	HAL_NVIC_SystemReset();
}

static scpi_result_t SCPI_DisplayBrightnessQ(scpi_t *context){

	return SCPI_RES_OK;
}
static scpi_result_t SCPI_DisplayBrightness(scpi_t *context){
	uint32_t brightness = 101;
	if(!SCPI_ParamUInt32(context, &brightness, 1)) return SCPI_RES_ERR;

	if((brightness <= 100) && (brightness >= 0)){
		Display_SetBrightness((uint8_t) brightness);
	} else {
		return SCPI_RES_ERR;
	}

	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayStateQ(scpi_t *context){
	return SCPI_RES_OK;
}

static scpi_result_t SCPI_DisplayState(scpi_t *context){
	bool state = 1;
	if (!SCPI_ParamBool(context, &state, 1)) return SCPI_RES_ERR;

	if (state){
		DisplayOn();
	} else {
		DisplayOff();
	}
	return SCPI_RES_OK;
}
