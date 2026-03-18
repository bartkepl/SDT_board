/*
 * mainApp.c
 *
 *  Created on: 16 mar 2026
 *      Author: bartkepl
 */


#include <mainApp.h>
#include <temp_sensor.h>
#include <dlr2416.h>

float temp =0;

void AppInit(void){
	if(TempSensors_Init(&hi2c1) != HAL_OK){
		Error_Handler();
	}
	//DLR2416_Init();
	//DLR2416_ClearAll();
}


void AppMain(void){
	temp = TempSensor_Read(1);
	//Display_ShowTemperature(0);
	HAL_Delay(800);
}
