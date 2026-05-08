# API czujników (C)

Dokumentacja publicznych funkcji i typów danych modułów sensor, tmp117 i sht45.

---

## Typy danych

### `SensorType_t` (`sensor.h`)

Enum identyfikujący aktywny czujnik.

```c
typedef enum {
    SENSOR_NONE  = 0,   // Żaden czujnik nie wykryty
    SENSOR_TMP117,      // Tylko TMP117
    SENSOR_SHT45,       // Tylko SHT45
    SENSOR_DUAL,        // Oba czujniki
    SENSOR_ERROR,       // Błąd komunikacji z czujnikiem
} SensorType_t;
```

### `SensorError_t` (`sensor.h`)

Enum błędów generowanych przez moduł sensor. Propagowane przez `SCPI_Main_Poll()` do kolejki błędów SCPI.

```c
typedef enum {
    SENSOR_ERR_NONE      = 0,  // Brak błędu
    SENSOR_ERR_NOT_FOUND,      // Czujnik nie wykryty → SCPI -241
    SENSOR_ERR_COMM,           // Błąd I2C           → SCPI -240
    SENSOR_ERR_TIMEOUT,        // Timeout DMA        → SCPI -365
    SENSOR_ERR_DATA,           // Błąd CRC / dane    → SCPI -230
} SensorError_t;
```

### `SensorData_t` (`sensor.h`)

Globalna struktura z danymi pomiarowymi. Dostępna jako `g_sensor`.

```c
typedef struct {
    SensorType_t type;          // Aktualny typ czujnika
    float        fTemp;         // Temperatura [°C]
    float        fHum;          // Wilgotność [%RH] (NaN dla TMP117)
    uint32_t     usSensorId;    // ID czujnika (serial number)
    uint8_t      ucValidFlag;   // 1 = dane ważne i aktualne
    uint8_t      ucNewDataFlag; // 1 = nowe dane dostępne
    uint8_t      ucInitializedFlag; // 1 = czujnik zainicjowany
    uint16_t     sMeasTaskTimer;    // Timer zadań pomiarowych
} SensorData_t;

extern SensorData_t g_sensor;
```

### `TMP117_Mode_t` (`tmp117.h`)

```c
typedef enum {
    TMP117_MODE_CONTINUOUS = 0x00,  // Ciągły pomiar
    TMP117_MODE_SHUTDOWN   = 0x01,  // Wyłączony
    TMP117_MODE_ONESHOT    = 0x03,  // Jeden pomiar
} TMP117_Mode_t;
```

### `TMP117_Averaging_t` (`tmp117.h`)

```c
typedef enum {
    TMP117_AVG_1  = 0x00,   // Brak uśredniania
    TMP117_AVG_8  = 0x01,   // 8x (domyślny)
    TMP117_AVG_32 = 0x02,   // 32x
    TMP117_AVG_64 = 0x03,   // 64x
} TMP117_Averaging_t;
```

### `SHT45_Precision_t` (`sht45.h`)

```c
typedef enum {
    SHT45_PRECISION_HIGH   = 0xFD,  // ~125 ms (domyślna)
    SHT45_PRECISION_MEDIUM = 0xF6,  // ~50 ms
    SHT45_PRECISION_LOW    = 0xE0,  // ~13 ms
} SHT45_Precision_t;
```

### `SHT45_HeaterMode_t` (`sht45.h`)

```c
typedef enum {
    SHT45_HEATER_200MW_1S    = 0x39,
    SHT45_HEATER_200MW_100MS = 0x32,
    SHT45_HEATER_110MW_1S    = 0x2F,
    SHT45_HEATER_110MW_100MS = 0x24,
    SHT45_HEATER_20MW_1S     = 0x1E,  // używana przez SCPI
    SHT45_HEATER_20MW_100MS  = 0x15,
} SHT45_HeaterMode_t;
```

---

## Funkcje warstwy `sensor.h`

### Inicjalizacja i zadania

```c
void Sensor_Init(I2C_HandleTypeDef *hi2c);
```
Inicjalizuje moduł sensor. Próbuje wykryć TMP117 i/lub SHT45 na magistrali I2C. Ustawia `g_sensor.type`.

```c
void Sensor_Task(void);
```
Główne zadanie czujnika — wywołuj co 1–10 ms w pętli głównej. Obsługuje maszyny stanów TMP117 i SHT45.

```c
SensorError_t Sensor_GetAndClearError(void);
```
Zwraca ostatni błąd i zeruje go. Wywoływana przez `SCPI_Main_Poll()` do propagacji błędów do SCPI.

### Callbacki I2C (wywoływane z przerwań HAL)

```c
void Sensor_I2C_TxComplete_Callback(void);
void Sensor_I2C_RxComplete_Callback(void);
void Sensor_I2C_Error_Callback(void);
```

---

## Funkcje SHT45 — `sensor.h`

### Konfiguracja

```c
void     Sensor_SHT45_SetReadPeriod(uint16_t periodMs);
uint16_t Sensor_SHT45_GetReadPeriod(void);
```
Ustawia/pobiera okres pomiarów SHT45 w milisekundach (zakres: 50–60000 ms).

```c
void    Sensor_SHT45_SetAverageCount(uint8_t count);
uint8_t Sensor_SHT45_GetAverageCount(void);
```
Ustawia/pobiera liczbę próbek do softwarowego uśredniania (zakres: 1–255).

```c
void    Sensor_SHT45_SetMeasurementPrecision(uint8_t precision);
uint8_t Sensor_SHT45_GetMeasurementPrecision(void);
```
Ustawia/pobiera precyzję pomiaru — wartości z `SHT45_Precision_t`.

### Komendy

```c
void Sensor_SHT45_RequestHeater(uint8_t ucHeaterMode);
```
Zleca aktywację grzałki. `ucHeaterMode` to wartość z `SHT45_HeaterMode_t`.

```c
void Sensor_SHT45_RequestSoftReset(void);
```
Zleca programowy reset SHT45.

---

## Funkcje TMP117 — `sensor.h`

### Tryb i konfiguracja konwersji

```c
void    Sensor_TMP117_SetMode(uint8_t mode);
uint8_t Sensor_TMP117_GetMode(void);
```
Tryb konwersji — wartości z `TMP117_Mode_t`.

```c
void    Sensor_TMP117_SetConvRate(uint8_t rate);
uint8_t Sensor_TMP117_GetConvRate(void);
```
Cykl konwersji CONV[2:0] — zakres 0–7.

```c
void    Sensor_TMP117_SetAvgHW(uint8_t avg);
uint8_t Sensor_TMP117_GetAvgHW(void);
```
Sprzętowe uśrednianie — wartości z `TMP117_Averaging_t` (0=1x, 1=8x, 2=32x, 3=64x).

```c
void     Sensor_TMP117_SetReadPeriod(uint16_t periodMs);
uint16_t Sensor_TMP117_GetReadPeriod(void);
```
Okres pomiarów w ms (zakres: 50–60000 ms).

### Alerty temperatury

```c
void  Sensor_TMP117_SetAlertHigh(float fTemp);
float Sensor_TMP117_GetAlertHigh(void);

void  Sensor_TMP117_SetAlertLow(float fTemp);
float Sensor_TMP117_GetAlertLow(void);
```
Progi alertu w °C. Zakres: -55,0 do +150,0 °C.

```c
uint8_t Sensor_TMP117_GetAlertStatus(void);
```
Odczyt statusu alertu. Zwraca bitfield:

| Bit | Znaczenie |
|-----|-----------|
| bit 1 (`0x02`) | HIGH alert aktywny (T ≥ THIGH) |
| bit 0 (`0x01`) | LOW alert aktywny (T ≤ TLOW) |
| bit 2 (`0x04`) | DATA_READY (nowy pomiar gotowy) |

### Soft reset

```c
void Sensor_TMP117_RequestSoftReset(void);
```

---

## Funkcje pomocnicze — `Utils.h`

```c
const char *serial_get(void);
```
Zwraca **8-znakowy** ciąg hex. Obliczony jako FNV-1a hash 20 bajtów:
`HAL_GetDEVID()` (4B) + `HAL_GetREVID()` (4B) + `UID_Word0/1/2` (12B).  
Używany przez `*IDN?` i `SYSTem:ID? SHORT`.

```c
const char *serial_get_full(void);
```
Zwraca **40-znakowy** ciąg hex — bezpośredni dump tych samych 20 bajtów.  
Używany przez `SYSTem:ID? LONG`.

---

## Przykłady użycia

### Odczyt temperatury

```c
if (g_sensor.ucValidFlag && g_sensor.type != SENSOR_NONE) {
    float temp = g_sensor.fTemp;
    // użyj temperatury
}
```

### Konfiguracja TMP117 przez API

```c
// Tryb najszybszy: 15.5 ms konwersja, bez uśredniania
Sensor_TMP117_SetConvRate(0);
Sensor_TMP117_SetAvgHW((uint8_t)TMP117_AVG_1);
Sensor_TMP117_SetReadPeriod(50);

// Alert przy 80 °C
Sensor_TMP117_SetAlertHigh(80.0f);
Sensor_TMP117_SetAlertLow(-10.0f);
```

### Konfiguracja SHT45

```c
// Niska precyzja, szybki pomiar, bez uśredniania
Sensor_SHT45_SetMeasurementPrecision((uint8_t)SHT45_PRECISION_LOW);
Sensor_SHT45_SetAverageCount(1);
Sensor_SHT45_SetReadPeriod(100);

// Aktywuj grzałkę 20 mW na 1 s
Sensor_SHT45_RequestHeater((uint8_t)SHT45_HEATER_20MW_1S);
```
