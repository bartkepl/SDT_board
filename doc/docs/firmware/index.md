# Architektura Firmware

Firmware SDT Board napisany jest w języku C (STM32 HAL + bare-metal) i oparty na współbieżnym modelu zadań (cooperative multitasking) — bez RTOS-a.

---

## Diagram przepływu danych

```
USB Host (PC)
      │
      │  USB 2.0 / USB-TMC
      ▼
┌─────────────────┐
│   TinyUSB       │  usbtmc_app.c
│   (USB stack)   │  — odbiera bajty USB
└────────┬────────┘  — buforuje komendy SCPI
         │ dane tekstowe (SCPI)
         ▼
┌─────────────────┐
│  SCPI Parser    │  libscpi / scpi-def.c
│  (libscpi)      │  — parsuje komendy
└───┬─────────────┘  — wywołuje handlery
    │           │
    ▼           ▼
┌───────┐  ┌───────────┐
│Sensor │  │  Display  │  — ustaw jasność
│ API   │  │  API      │  — ustaw tekst
│       │  │           │  — zmień źródło
└───┬───┘  └─────┬─────┘
    │             │
    ▼             ▼
┌─────────────────────────────────┐
│         I2C + GPIO              │
│  TMP117  │  SHT45  │  DLR2416  │
└─────────────────────────────────┘
```

---

## Pętla główna (main loop)

`Core/Src/main.c` — uruchamia po inicjalizacji peryferiów nieskończoną pętlę zadań:

```c
while (1) {
    tud_task();                /* TinyUSB — obsługa USB */
    usbtmc_app_task_iter();    /* USB-TMC — SCPI request/response */
    Sensor_Task();             /* Czujnik — maszyna stanów I2C */
    SCPI_Main_Poll();          /* SCPI — propagacja błędów czujnika */
    Display_task();            /* Wyświetlacz — odświeżanie LED */
}
```

Wszystkie zadania są **nieblokujące** (non-blocking) — każde sprawdza swój stan, wykonuje krok i oddaje sterowanie. Synchronizacja oparta na 1 ms timerze systemowym (`SysTick`).

---

## Inicjalizacja

Kolejność inicjalizacji przy starcie (`main.c`):

```
── HAL (generowane przez CubeMX) ──────────────────────────────────
 1. HAL_Init()            — inicjalizacja HAL STM32
 2. SystemClock_Config()  — HSI48 48 MHz + CRS synchronizacja z USB
 3. MX_GPIO_Init()        — GPIO (D0-D6, A0-A1, CE, WR, CLR, LED)
 4. MX_DMA_Init()         — DMA1 CH1/CH2 (I2C TX/RX)
 5. MX_I2C1_Init()        — I2C1 (timing 0x00402D41)
 6. MX_TIM3_Init()        — TIM3 PWM CH2 (podświetlenie, ARR=255)
 7. MX_USB_PCD_Init()     — USB DRD FS
 8. MX_CRC_Init()         — sprzętowy CRC

── Kod użytkownika ─────────────────────────────────────────────────
 9. Sensor_Init(&hi2c1)   — detekcja TMP117/SHT45, konfiguracja I2C
10. tud_init()            — TinyUSB stack
11. SCPI_Main_Init()      — parser SCPI + rejestracja komend + IDN
12. Config_Init()         — wczytanie konfiguracji z FLASH (DEFAULT/PRIMARY/BACKUP)
13. Display_Init()        — DLR2416_Init() + zastosowanie zapisanej jasności
```

---

## Moduły firmware

### `Core/Src/main.c`
Punkt wejścia. Inicjalizacja peryferiów i pętla główna.

### `Core/Src/usbtmc_app.c`
Obsługa protokołu USB-TMC:
- Odbieranie pakietów BULK OUT (komendy SCPI)
- Wysyłanie pakietów BULK IN (odpowiedzi SCPI)
- Zarządzanie stanem request/response (maszyna stanów)
- Wywołanie `SCPI_Main_Input()` gdy komenda jest kompletna

### `App/libscpi/src/scpi-def.c`
Rejestracja i implementacja wszystkich komend SCPI:
- Lista komend (`scpi_commands[]`) — mapowanie wzorców na handlery
- Implementacja 30+ handlerów C
- Propagacja błędów do kolejki SCPI
- Integracja z modułami sensor i display

### `App/sensor/sensor.c`
Zunifikowany interfejs czujnika:
- Auto-detekcja TMP117 i SHT45 na I2C podczas `Sensor_Init()`
- `Sensor_Task()` — wywołuje `TMP117_Task()` lub `SHT45_Task()` w zależności od wykrytego czujnika
- `g_sensor` — globalny obiekt z danymi (typ, temperatura, wilgotność, flagi)
- Propagacja błędów przez `Sensor_GetAndClearError()`
- Arbitraż magistrali I2C przez `g_i2c_active_sensor`

### `App/sensor/tmp117.c`
Sterownik TMP117:
- Maszyna stanów (26 stanów) obsługująca wszystkie transakcje I2C
- Asynchroniczny odczyt przez DMA — `TMP117_I2C_TxComplete_Callback()` / `TMP117_I2C_RxComplete_Callback()`
- Odczyt: temperatura, ID, CONFIG, THIGH, TLOW
- Zapis: CONFIG (tryb/AVG/CONV), THIGH, TLOW
- Obsługa timeout DMA (100 ms)

### `App/sensor/sht45.c`
Sterownik SHT45:
- Maszyna stanów (22 stany) obsługująca pomiar, serial, reset i grzałkę
- Asynchroniczny odczyt przez DMA
- Walidacja CRC-8 dla każdego pomiaru
- Softwarowe uśrednianie (akumulator próbek)
- Timeout DMA (100 ms)

### `App/D2416/display.c`
Zarządzanie wyświetlaczem:
- `Display_task()` — wywoływany co 100 ms, odświeża wyświetlacz
- Wybór źródła: pomiar (auto-format) lub tekst użytkownika
- Kolejka błędów (4 wpisy FIFO), każdy wyświetlany 1500 ms
- Sterowanie jasnością przez `DLR2416_SetBrightness()`

### `App/D2416/dlr2416.c`
Niskopoziomowy sterownik DLR2416:
- Bit-banged GPIO dla szyny danych i sygnałów sterujących
- `DLR2416_WriteChar()` — zapis jednego znaku na pozycję
- `DLR2416_WriteString8()` — zapis 8 znaków na cały wyświetlacz
- `DLR2416_SetBrightness()` — sterowanie PWM TIM3
- `DLR2416_PWM_Enable()` / `DLR2416_PWM_Disable()` — włącz/wyłącz

### `App/Flash/flash.c`
Trójwarstwowy system przechowywania konfiguracji w FLASH EEPROM:

- **Układ pamięci FLASHEE (6 KB = 3 × 2 KB strony):**

  | Strona | Adres | Region | Opis |
  |--------|-------|--------|------|
  | 56 | `0x0801C000` | **DEFAULT** | Wartości fabryczne — const, część firmware |
  | 57 | `0x0801C800` | **PRIMARY** | Aktywna konfiguracja (runtime writable) |
  | 58 | `0x0801D000` | **BACKUP** | Kopia PRIMARY (runtime writable) |

- `Config_Init()` — wczytuje PRIMARY (CRC OK) → BACKUP (CRC OK, naprawa PRIMARY) → DEFAULT (magic OK) → fallback
- `Config_Apply()` — stosuje `g_config` do sterowników display i sensor
- `Config_Capture()` — snapshot bieżącego stanu sterowników do `g_config`
- `Config_Save()` — `Config_Capture()` + obliczenie CRC + zapis PRIMARY + zapis BACKUP
- `Config_Restore()` — BACKUP → PRIMARY + `Config_Apply()`
- `Config_Recall()` — DEFAULT → PRIMARY + BACKUP + `Config_Apply()`
- `Config_MarkDirty()` / `Config_IsDirty()` — zarządzanie flagą niezapisanych zmian

  Każdy blok 40 bajtów chroniony przez CRC-32 (sprzętowe peryferium CRC STM32). DEFAULT walidowany tylko po magic word (`0x5344544B`).

### `App/Utils/Utils.c`
Funkcje pomocnicze:
- `SysTimZeroTimer1ms_u16()` — start timera 1 ms
- `SysTimTestTimer1ms_u16()` — sprawdzenie czy minął czas (wrap-safe, 16-bit)
- `serial_get()` — **8-znakowy** FNV-1a hash z 20 B danych MCU (DEVID+REVID+UID)
- `serial_get_full()` — **40-znakowy** hex dump tych 20 bajtów

---

## Obsługa błędów

```
Sensor_Task()
    │
    ├── Błąd I2C → ustaw SENSOR_ERR_COMM
    ├── Timeout DMA → ustaw SENSOR_ERR_TIMEOUT
    └── Błąd CRC → ustaw SENSOR_ERR_DATA

SCPI_Main_Poll()  (wywoływany co iterację)
    │
    └── Sensor_GetAndClearError()
           │
           ├── SENSOR_ERR_NOT_FOUND → SCPI_ErrorPush(-241)
           ├── SENSOR_ERR_COMM → SCPI_ErrorPush(-240)
           ├── SENSOR_ERR_TIMEOUT → SCPI_ErrorPush(-365)
           └── SENSOR_ERR_DATA → SCPI_ErrorPush(-230)

SCPI_Error callback
    │
    └── Display_ShowError(code)  → wyświetl "ERR:XXXX" przez 1.5s
```

---

## Okresy zadań

| Zadanie | Okres wywołania | Uwagi |
|---------|----------------|-------|
| `tud_task()` | Co iterację pętli | USB — as fast as possible |
| `usbtmc_app_task_iter()` | Co iterację pętli | USB-TMC state machine |
| `Sensor_Task()` | Co iterację pętli | Wewnętrzny timer 10 ms |
| `SCPI_Main_Poll()` | Co iterację pętli | Sprawdza błędy czujnika |
| `Display_task()` | Co iterację pętli | Wewnętrzny timer 100 ms |
| Odczyt czujnika | 1000 ms domyślnie | Konfigurowalny przez SCPI |

---

## Linki do API

- [API czujników](sensor-api.md) — funkcje C dla TMP117, SHT45 i warstwy sensor
- [API wyświetlacza](display-api.md) — funkcje C dla display i DLR2416
