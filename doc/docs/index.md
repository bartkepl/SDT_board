# SDT Board — Dokumentacja

**SDT** (SCPI Display Thermometer) to USB-termometr laboratoryjny z interfejsem SCPI, zbudowany na mikrokontrolerze STM32C071CBT6. Urządzenie mierzy temperaturę (i opcjonalnie wilgotność), wyświetla wyniki na 8-znakowym wyświetlaczu LED i udostępnia pełny interfejs sterowania przez USB.

---

## Przegląd systemu

```
┌─────────────────────────────────────────────────────────┐
│                     SDT Board                           │
│                                                         │
│  ┌──────────┐   I2C    ┌──────────┐  ┌──────────────┐  │
│  │  TMP117  │◄────────►│          │  │  DLR2416     │  │
│  │  (temp)  │          │STM32C071 │  │  8-znak LED  │  │
│  └──────────┘   I2C    │  CBT6    │─►│  wyświetlacz │  │
│  ┌──────────┐◄────────►│          │  └──────────────┘  │
│  │  SHT45   │          │          │                     │
│  │ (T + RH) │          │  USB-TMC │                     │
│  └──────────┘          └────┬─────┘                     │
│                             │                           │
└─────────────────────────────┼───────────────────────────┘
                              │ USB 2.0
                       ┌──────▼──────┐
                       │     PC      │
                       │  PyVISA /   │
                       │  LabVIEW /  │
                       │  Terminal   │
                       └─────────────┘
```

---

## Parametry urządzenia

| Cecha | Wartość |
|-------|---------|
| MCU | STM32C071CBT6 (ARM Cortex-M0+, 48 MHz) |
| Flash | 128 KB |
| RAM | 12 KB |
| Komunikacja | USB 2.0 Device — klasa USB-TMC |
| Protokół | SCPI (IEEE 488.2) |
| Identyfikacja | `bartkepl,SDT,<serial>,1.0` |
| Czujniki | TMP117 i/lub SHT45 (auto-detekcja) |
| Wyświetlacz | 8-znakowy LED dot-matrix (DLR2416 × 2) |
| Jasność | Regulowana PWM 1–100% |

---

## Obsługiwane czujniki

| Czujnik | Temperatura | Wilgotność | Adres I2C | Uwagi |
|---------|-------------|------------|-----------|-------|
| TMP117 | -55 °C … +150 °C, ±0,1 °C | — | 0x48 | Wysokoprecyzyjny, alerty T |
| SHT45 | -40 °C … +125 °C | 0–100 % RH | 0x44 | Temp + wilgotność, grzałka |

Urządzenie wykrywa czujniki automatycznie podczas uruchamiania. Gdy oba są podłączone, działa w trybie **DUAL** — dane temperatury i wilgotności są zbierane z SHT45, a TMP117 może być używany równolegle.

---

## Szybki start

```bash
pip install pyvisa pyvisa-py
```

```python
import pyvisa
rm = pyvisa.ResourceManager('@py')
# VID=0xCAFE, PID=0x4000, numer seryjny USB: "SDT_<8charHash>"
inst = rm.open_resource('USB0::0xCAFE::0x4000::SDT_YOURHASH::INSTR')
print(inst.query('*IDN?'))
print(inst.query('SENSor:TEMPerature?'))
```

Więcej przykładów: [Pierwsze kroki](getting-started.md)

---

## Komendy SCPI

Urządzenie obsługuje ponad 30 komend SCPI:

- **[IEEE 488.2](scpi/ieee488.md)** — `*IDN?`, `*RST`, `*TST?` i inne standardowe
- **[SYSTem](scpi/system.md)** — błędy, wersja, ID MCU, reset, bootloader DFU
- **[SENSor](scpi/sensor.md)** — temperatura, wilgotność, tryby pracy, alerty
- **[DISPlay](scpi/display.md)** — jasność, stan, tekst, źródło danych

---

## Zasoby zewnętrzne

- Biblioteka SCPI: [scpi-parser (j123b567)](https://github.com/j123b567/scpi-parser)
- Stos USB: [TinyUSB](https://github.com/hathach/tinyusb)
- Inspiracja projektem: [Sebastian Harnisch — SCPI Thermometer](https://sebastianharnisch.de/scpi-enabled-thermometer/)
