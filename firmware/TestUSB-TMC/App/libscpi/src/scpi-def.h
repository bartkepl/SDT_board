/*
 * scpi_def.h
 *
 *  Created on: Dec 21, 2025
 *      Author: Bart
 */

#ifndef INC_SCPI_DEF_H_
#define INC_SCPI_DEF_H_

#include <scpi/scpi.h>
#include <stdint.h>

#define SCPI_IDN_MANUFACTURER "bartkepl"
#define SCPI_IDN_MODEL        "SDT"
#define SCPI_IDN_SERIAL       "0001"
#define SCPI_IDN_FW           "1.0"

extern scpi_t scpi_context;

void SCPI_Main_Init(void);
void SCPI_Main_Input(const char *data, uint32_t len);

#endif /* INC_SCPI_DEF_H_ */
