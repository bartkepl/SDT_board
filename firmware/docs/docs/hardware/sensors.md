# Czujniki

SDT Board obsługuje dwa czujniki temperatury: **TMP117** (temperatura) i **SHT45** (temperatura + wilgotność). Oba używają tego samego magistrali I2C. Wykrywane są automatycznie podczas uruchamiania — można też używać obydwu jednocześnie (tryb DUAL).

---

## TMP117 — Czujnik temperatury

### Parametry elektryczne

| Parametr | Wartość |
|----------|---------|
| Adres I2C | `0x48` (8-bit: `0x90`) |
| Napięcie zasilania | 1,7–5,5 V |
| Pobór prądu | 3,5 µA @ 1 pom./s |
| Zakres temperatury | -55 °C … +150 °C |
| Dokładność typowa | ±0,1 °C (0–70 °C) |
| Rozdzielczość | **7,8125 m°C/LSB** (0,0078125 °C) |
| Interfejs | I2C (standardowy / fast mode) |

### Konwersja temperatury

Temperatura przechowywana jest w rejestrze `0x00` jako 16-bitowa liczba ze znakiem (uzupełnienie do dwóch):

```
T [°C] = raw_int16 × 0.0078125
```

**Przykład:** `raw = 0x0C80` = 3200 → T = 3200 × 0.0078125 = **25,00 °C**

### Rejestry

| Adres | Nazwa | Dostęp | Opis |
|-------|-------|--------|------|
| `0x00` | TEMP | R | Wynik pomiaru temperatury |
| `0x01` | CONFIG | R/W | Konfiguracja: tryb, CONV, AVG, alerty |
| `0x02` | THIGH | R/W | Górny próg alertu |
| `0x03` | TLOW | R/W | Dolny próg alertu |
| `0x0F` | ID | R | Identyfikator układu |

### Rejestr CONFIG (0x01) — Mapa bitów

| Bity | Nazwa | Opis |
|------|-------|------|
| [15] | HIGH_ALERT | R — flaga: temperatura ≥ THIGH |
| [14] | LOW_ALERT | R — flaga: temperatura ≤ TLOW |
| [13] | DATA_READY | R — nowy pomiar gotowy |
| [12] | EEPROM_BUSY | R — EEPROM zajęty |
| [11:10] | MOD[1:0] | R/W — tryb konwersji |
| [9:7] | CONV[2:0] | R/W — cykl konwersji |
| [6:5] | AVG[1:0] | R/W — sprzętowe uśrednianie |
| [1] | SOFT_RESET | W — reset programowy (auto-zeruje się) |

**Domyślna wartość CONFIG:** `0x0220` (MOD=Continuous, CONV=1s, AVG=8x)

### Tryby konwersji (MOD)

| Wartość | SCPI | Opis |
|---------|------|------|
| `0x00` | `CONTinuous` | Ciągły pomiar |
| `0x01` | `SHUTdown` | Wyłączony — minimalne zużycie energii |
| `0x03` | `ONESHot` | Jeden pomiar, potem shutdown |

### Cykl konwersji (CONV) a tryb uśredniania (AVG)

Łączny czas pomiaru = czas konwersji × liczba próbek AVG:

| CONV kod | Czas (AVG=1) | Czas (AVG=8) | Czas (AVG=32) | Czas (AVG=64) |
|---------|-------------|-------------|--------------|--------------|
| 0 | 15,5 ms | 125 ms | 500 ms | 1 s |
| 1 | 125 ms | 125 ms | 500 ms | 1 s |
| 2 | 250 ms | 250 ms | 500 ms | 1 s |
| 3 | 500 ms | 500 ms | 500 ms | 1 s |
| 4 | 1 s | 1 s | 1 s | 1 s |
| 5 | 4 s | 4 s | 4 s | 4 s |
| 6 | 8 s | 8 s | 8 s | 8 s |
| 7 | 16 s | 16 s | 16 s | 16 s |

### Alerty temperatury

TMP117 posiada sprzętowe alerty temperatury — wywoływane, gdy temperatura przekroczy progi THIGH lub TLOW. Progi przechowywane są w rejestrach i używają tego samego formatu co TEMP.

```python
inst.write('SENSor:ALERt:HIGH 80.0')    # alert przy T > 80 °C
inst.write('SENSor:ALERt:LOW -10.0')    # alert przy T < -10 °C
status = inst.query('SENSor:ALERt:STATus?')
```

---

## SHT45 — Czujnik temperatury i wilgotności

### Parametry elektryczne

| Parametr | Wartość |
|----------|---------|
| Adres I2C | `0x44` (8-bit: `0x88`) |
| Napięcie zasilania | 1,08–3,6 V |
| Pobór prądu | ~1 µA (standby) |
| Zakres temperatury | -40 °C … +125 °C |
| Zakres wilgotności | 0 % … 100 % RH |
| Dokładność T (typowa) | ±0,1 °C |
| Dokładność RH (typowa) | ±1,0 % RH |
| Interfejs | I2C |

### Konwersja pomiarów

```
T [°C] = -45 + 175 × (raw_T / 65535)
H [%RH] = -6 + 125 × (raw_H / 65535)
```

Każdy pomiar zawiera **CRC-8** dla walidacji integralności danych (generowany i sprawdzany przez firmware).

### Komendy SHT45

| Komenda (hex) | Stała | Opis | Czas pomiaru |
|--------------|-------|------|-------------|
| `0xFD` | `SHT45_PRECISION_HIGH` | Pomiar — wysoka precyzja | ~125 ms |
| `0xF6` | `SHT45_PRECISION_MEDIUM` | Pomiar — średnia precyzja | ~50 ms |
| `0xE0` | `SHT45_PRECISION_LOW` | Pomiar — niska precyzja | ~13 ms |
| `0x89` | — | Odczyt numeru seryjnego | — |
| `0x94` | — | Soft reset | — |

### Grzałka (heater)

SHT45 posiada wbudowaną grzałkę do usuwania kondensacji lub testowania odpowiedzi czujnika:

| Stała | Hex | Moc | Czas | Opis |
|-------|-----|-----|------|------|
| `SHT45_HEATER_200MW_1S` | `0x39` | 200 mW | 1 s | Maksymalna moc, długi czas |
| `SHT45_HEATER_200MW_100MS` | `0x32` | 200 mW | 100 ms | Maksymalna moc, krótki czas |
| `SHT45_HEATER_110MW_1S` | `0x2F` | 110 mW | 1 s | Średnia moc, długi czas |
| `SHT45_HEATER_110MW_100MS` | `0x24` | 110 mW | 100 ms | Średnia moc, krótki czas |
| `SHT45_HEATER_20MW_1S` | `0x1E` | 20 mW | 1 s | **Domyślna** (SCPI) |
| `SHT45_HEATER_20MW_100MS` | `0x15` | 20 mW | 100 ms | Minimalna moc, krótki czas |

!!! warning "Po użyciu grzałki"
    Wyniki pomiarów po zakończeniu grzania są zawyżone. Odczekaj kilka sekund zanim użyjesz danych pomiarowych.

---

## Tryb DUAL — obydwa czujniki

Gdy oba czujniki są wykryte podczas inicjalizacji:

- **Dane temperatury** — pobierane z SHT45 (priorytet — ma też wilgotność)
- **Dane wilgotności** — pobierane z SHT45
- **TMP117** — działa równolegle (alerty, konfiguracja dostępna przez SCPI)
- **Arbitraż magistrali I2C** — globalny mutex `g_i2c_active_sensor` zapobiega konfliktom

---

## Auto-detekcja czujników

Podczas inicjalizacji firmware próbuje nawiązać komunikację z obydwoma adresami I2C:

1. Próba połączenia z TMP117 (`0x48`)
2. Próba połączenia z SHT45 (`0x44`)
3. Timeout detekcji: **5 sekund**
4. Jeśli żaden czujnik nie odpowie → SCPI błąd `-241` (Hardware missing)

```
*TST?  →  0   (czujnik wykryty i sprawny)
*TST?  →  1   (brak czujnika)
```

---

## Porównanie czujników

| Cecha | TMP117 | SHT45 |
|-------|--------|-------|
| Temperatura | ✓ -55…+150 °C | ✓ -40…+125 °C |
| Wilgotność | ✗ | ✓ 0–100 %RH |
| Rozdzielczość T | 0,0078125 °C | zależy od precyzji |
| Sprzętowe uśrednianie | 1/8/32/64x | ✗ (softwarowe) |
| Softwarowe uśrednianie | ✗ | 1–255x |
| Alerty T | ✓ THIGH/TLOW | ✗ |
| Grzałka | ✗ | ✓ |
| Tryby konwersji | 3 tryby | precyzja LOW/MED/HIGH |
| CRC walidacja | ✗ | ✓ (CRC-8) |
