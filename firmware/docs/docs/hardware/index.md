# Sprzęt — Przegląd

SDT Board zbudowany jest na mikrokontrolerze **STM32C071CBT6** (ARM Cortex-M0+) z trzema głównymi interfejsami sprzętowymi: I2C do czujników, USB do komunikacji z komputerem i GPIO do sterowania wyświetlaczem.

---

## Mikrokontroler

| Parametr | Wartość |
|----------|---------|
| Model | STM32C071CBT6 |
| Rdzeń | ARM Cortex-M0+ |
| Częstotliwość | 48 MHz (HSI48 + CRS — synchronizacja z USB) |
| Flash | 128 KB |
| RAM | 12 KB |
| Obudowa | LQFP48 |
| Narzędzie konfiguracji | STM32CubeMX (plik `SDT_board.ioc`) |

---

## Interfejsy i peryferia

| Peryferium | Zastosowanie | Uwagi |
|-----------|-------------|-------|
| **I2C1** | Komunikacja z czujnikami | DMA-enabled (TX: DMA1_CH1, RX: DMA1_CH2) |
| **USB** | USB 2.0 Device (USB-TMC) | Zasilany z USB, synchronizacja zegara przez CRS |
| **TIM3 CH2** | PWM sterowanie jasnością wyświetlacza | AF11, PC14, ARR=255, formuła CCR=1199×%/100 |
| **CRC** | Walidacja danych z SHT45 | Sprzętowy kalkulator CRC |
| **DMA1_CH1/CH2** | Transmisja I2C | Odciąża CPU podczas pomiaru |

---

## Mapa pinów GPIO

### Port A — szyna danych wyświetlacza

| Pin | Sygnał | Kierunek | Opis |
|-----|--------|----------|------|
| PA0 | D0 | Out | Bit 0 szyny danych wyświetlacza |
| PA1 | D1 | Out | Bit 1 szyny danych wyświetlacza |
| PA2 | D2 | Out | Bit 2 szyny danych wyświetlacza |
| PA3 | D3 | Out | Bit 3 szyny danych wyświetlacza |
| PA4 | D4 | Out | Bit 4 szyny danych wyświetlacza |
| PA5 | D5 | Out | Bit 5 szyny danych wyświetlacza |
| PA6 | D6 | Out | Bit 6 szyny danych wyświetlacza |
| PA7 | A0 | Out | Linia adresowa 0 (wybór pozycji) |
| PA10 | LED | Out | Dioda LED statusu |

### Port B — sterowanie wyświetlaczem

| Pin | Sygnał | Kierunek | Logika | Opis |
|-----|--------|----------|--------|------|
| PB0 | A1 | Out | — | Linia adresowa 1 (wybór pozycji) |
| PB1 | WR | Out | Aktywny LOW | Zapis — strob zapisu |
| PB2 | CLR | Out | Aktywny LOW | Czyszczenie pamięci wyświetlacza |
| PB3 | CE1_1 | Out | Aktywny LOW | Chip Enable 1 wyświetlacza 1 |
| PB4 | CE2_1 | Out | Aktywny LOW | Chip Enable 2 wyświetlacza 1 |
| PB5 | CE1_2 | Out | Aktywny LOW | Chip Enable 1 wyświetlacza 2 |
| PB6 | CE2_2 | Out | Aktywny LOW | Chip Enable 2 wyświetlacza 2 |
| PB12 | CU | Out | Aktywny LOW | Chip Unit — tryb DANE/KOMENDA |
| PB13 | CUE | Out | — | Chip Unit Enable — zatrzask komendy |

### Port C

| Pin | Sygnał | Kierunek | Opis |
|-----|--------|----------|------|
| PC14 | BL | PWM Out | Podświetlenie LED — TIM3 CH2, AF11 |

---

## Topologia I2C

```
STM32C071
    │
    ├── I2C1 (DMA) ─── SDA/SCL
    │                      │
    │              ┌────────┴────────┐
    │           TMP117            SHT45
    │          addr 0x48         addr 0x44
    │          (temp)            (T + RH)
```

Oba czujniki dzielą jeden magistralę I2C. W trybie DUAL używany jest globalny mutex (`g_i2c_active_sensor`) zapobiegający konfliktu na szynie.

---

## Schemat zasilania

| Napięcie | Źródło |
|----------|--------|
| VBUS 5V | USB |
| VDD 3,3V | Regulator na płytce |

---

## Linki

- [Czujniki](sensors.md) — TMP117 i SHT45 — specyfikacje i tryby pracy
- [Wyświetlacz DLR2416](display.md) — protokół i pinout
