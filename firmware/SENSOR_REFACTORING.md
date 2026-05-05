# Sensor Module Refactoring - Non-blocking State Machine with I2C DMA

## Overview
The sensor module has been refactored from blocking I2C polling to a non-blocking state machine architecture using DMA transfers. This provides better real-time performance and allows the system to handle other tasks concurrently.

## Architecture

### State Machines

#### TMP117 (Temperature Sensor)
```
IDLE → REQ_TEMP → REC_TEMP → REQ_ID → REC_ID → REQ_CONFIG → REC_CONFIG → IDLE
```
- **Measurement Period**: 1000ms
- **Task Period**: 10ms
- **Data Read**: Temperature, Device ID, Configuration Register

#### SHT45 (Humidity & Temperature Sensor)
```
IDLE ↔ REQ_MEASURE → REC_MEASURE → REQ_SERIAL → REC_SERIAL
     ↔ REQ_HEATER → REC_HEATER
     ↔ REQ_SOFTRESET → WAIT_SOFTRESET → REQ_SOFTRESET_RESTART → WAIT_COMPLETE
```
- **Measurement Period**: 1000ms
- **Task Period**: 10ms
- **Precision Modes**: High (0xFD), Medium (0xF6), Low (0xE0)
- **Heater Modes**: 
  - 200mW/1s, 200mW/100ms
  - 110mW/1s, 110mW/100ms
  - 20mW/1s, 20mW/100ms

### DMA-Based I2C Operations

All I2C transfers use HAL DMA functions:
- `HAL_I2C_Master_Transmit_DMA()`
- `HAL_I2C_Master_Receive_DMA()`

Completion is handled through callbacks:
- `HAL_I2C_MasterTxCpltCallback()` - Routes to sensor handler
- `HAL_I2C_MasterRxCpltCallback()` - Routes to sensor handler
- `HAL_I2C_ErrorCallback()` - Error handling

## API Usage

### Sensor Initialization
```c
void Sensor_Init(I2C_HandleTypeDef *hi2c);
```

### Main Task Loop (call every 1-10ms)
```c
void Sensor_Task(void);
```

### I2C Callbacks (called from HAL/ISR)
```c
void Sensor_I2C_Complete_Callback(void);
void Sensor_I2C_Error_Callback(void);
```

### SHT45 Controls
```c
// Request heater operation
void Sensor_SHT45_RequestHeater(uint8_t ucHeaterMode);
// SHT45_HEATER_200MW_1S, etc.

// Request soft reset
void Sensor_SHT45_RequestSoftReset(void);

// Set measurement precision
void Sensor_SHT45_SetPrecision(uint8_t ucPrecision);
// SHT45_PRECISION_HIGH, SHT45_PRECISION_MEDIUM, SHT45_PRECISION_LOW
```

## Sensor Data Access

Via global structures:
```c
extern SensorData_t g_sensor;      // Unified interface
extern TMP117_Data_t g_tmp117;     // Direct TMP117 access
extern SHT45_Data_t g_sht45;       // Direct SHT45 access
```

## Integration Points

### In main.c
1. Initialize: `Sensor_Init(&hi2c1);`
2. Call task: `Sensor_Task();` (in main loop)
3. Ensure I2C is configured with DMA

### In stm32c0xx_it.c
I2C callbacks are already integrated and route to sensor module.

### In SCPI Interface
Updated `SCPI_SensorHeater()` to use new command structure.

## Timing Specifications

- **State Machine Period**: 10ms
- **Sensor Measurement Period**: 1000ms (configurable via TMP117_READ_PERIOD_MS, SHT45_READ_PERIOD_MS)
- **Wait Timers**: Various inter-operation delays (handled automatically)

## Performance Benefits

1. **Non-blocking**: No `HAL_Delay()` calls
2. **DMA-based**: CPU not blocked during I2C transfers
3. **Scalable**: Can easily add more sensors to the state machine
4. **Responsive**: Main loop continues running during sensor operations
5. **Error Tolerant**: Automatic retry on I2C errors

## Testing Notes

- Verify I2C DMA is enabled in CubeMX configuration
- Check that I2C1 DMA channels are properly configured
- Monitor I2C bus with oscilloscope to verify DMA transfers
- Confirm sensor data appears in expected timing
