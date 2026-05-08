# SENSor — Komendy czujnika

Komendy `SENSor:` służą do odczytu pomiarów i konfiguracji czujników temperatury i wilgotności. Część komend dostępna jest tylko dla określonego czujnika.

!!! info "Dostępność"
    Komenda może zwrócić błąd `-221` (`Settings conflict`), jeśli jest skierowana do czujnika, który jej nie obsługuje — np. `SENSor:HEATer` dla TMP117.

---

## Komendy ogólne

### `SENSor:TYPE?`

Zwraca typ aktualnie wykrytego czujnika.

**Składnia:** `SENSor:TYPE?`  
**Odpowiedź:** string

| Odpowiedź | Znaczenie |
|-----------|-----------|
| `"SHT45"` | Podłączony czujnik SHT45 (T + RH) |
| `"TMP117"` | Podłączony czujnik TMP117 (tylko T) |
| `"DUAL"` | Oba czujniki — TMP117 i SHT45 |
| `"UNKNWN"` | Brak czujnika lub błąd |

```python
sensor_type = inst.query('SENSor:TYPE?')
```

---

### `SENSor:TEMPerature?`

Odczytuje ostatnio zmierzoną temperaturę.

**Składnia:** `SENSor:TEMPerature?`  
**Odpowiedź:** float w stopniach Celsjusza

| Czujnik | Zakres | Rozdzielczość |
|---------|--------|---------------|
| TMP117 | -55 … +150 °C | 0,0078125 °C/LSB |
| SHT45 | -40 … +125 °C | zależy od trybu precyzji |

```python
temp = float(inst.query('SENSor:TEMPerature?'))
print(f'{temp:.4f} °C')
```

!!! warning "Błędy"
    - `-240` — błąd sprzętowy (I2C)
    - `-241` — czujnik nie wykryty
    - `-230` — dane nieważne (np. brak pierwszego pomiaru)

---

### `SENSor:HUMidity?`

Odczytuje ostatnio zmierzoną wilgotność względną.

**Składnia:** `SENSor:HUMidity?`  
**Odpowiedź:** float w procentach `%RH`, lub `NaN` dla TMP117

| Czujnik | Odpowiedź |
|---------|-----------|
| SHT45 | 0,0 … 100,0 %RH |
| TMP117 | `NaN` (czujnik nie mierzy wilgotności) |
| DUAL | jak SHT45 |

```python
hum = float(inst.query('SENSor:HUMidity?'))
if not math.isnan(hum):
    print(f'{hum:.2f} %RH')
```

---

### `SENSor:ID?`

Odczytuje numer seryjny czujnika.

**Składnia:** `SENSor:ID?`  
**Odpowiedź:** uint32 — numer seryjny (uint32)

| Czujnik | Źródło |
|---------|--------|
| TMP117 | Rejestr ID (adres 0x0F) |
| SHT45 | Numer seryjny 32-bit (komenda 0x89) |

```python
sensor_id = inst.query('SENSor:ID?')
```

---

### `SENSor:READperiod?` / `SENSor:READperiod`

Odczytuje lub ustawia okres między pomiarami czujnika.

**Składnia odczytu:** `SENSor:READperiod?`  
**Składnia zapisu:** `SENSor:READperiod <ms>`  
**Parametr:** uint — okres w milisekundach  
**Zakres:** 50 … 60000 ms  
**Wartość domyślna:** 1000 ms  
**Odpowiedź (odczyt):** uint (ms)

```python
print(inst.query('SENSor:READperiod?'))   # 1000
inst.write('SENSor:READperiod 500')        # ustaw 500 ms
```

!!! note
    Dotyczy okresu odświeżania firmware (jak często MCU odpytuje czujnik), a nie bezpośrednio prędkości konwersji sprzętowej.

---

### `SENSor:AVErage?` / `SENSor:AVErage`

Odczytuje lub ustawia liczbę próbek do uśredniania.

**Składnia odczytu:** `SENSor:AVErage?`  
**Składnia zapisu:** `SENSor:AVErage <count>`  
**Odpowiedź (odczyt):** uint

| Czujnik | Dopuszczalne wartości | Opis |
|---------|----------------------|------|
| SHT45 | 1–255 | Softwarowe uśrednianie N próbek |
| TMP117 | **tylko:** 1, 8, 32, 64 | Sprzętowe uśrednianie w konfiguracji układu |

```python
# SHT45: ustaw 10-krotne uśrednianie
inst.write('SENSor:AVErage 10')

# TMP117: ustaw 8-krotne (sprzętowe)
inst.write('SENSor:AVErage 8')
print(inst.query('SENSor:AVErage?'))   # 8
```

!!! warning "TMP117"
    Dla TMP117 dopuszczalne są **wyłącznie** wartości `1`, `8`, `32`, `64`. Inna wartość zwróci błąd `-222` (Data out of range).

---

### `SENSor:PRECision?` / `SENSor:PRECision`

Odczytuje lub ustawia precyzję pomiaru SHT45. Dla TMP117 odczyt zwraca `NaN`.

**Składnia odczytu:** `SENSor:PRECision?`  
**Składnia zapisu:** `SENSor:PRECision <mode>`  
**Czujnik:** SHT45 (dla TMP117: błąd `-221`)  
**Odpowiedź (odczyt):** string

| Wartość | Czas pomiaru | Komenda SHT45 |
|---------|-------------|---------------|
| `HIGH` | ~125 ms | `0xFD` |
| `MEDIUM` | ~50 ms | `0xF6` |
| `LOW` | ~13 ms | `0xE0` |

```python
print(inst.query('SENSor:PRECision?'))    # "HIGH"
inst.write('SENSor:PRECision MEDIUM')
```

---

### `SENSor:SOFTReset`

Wykonuje programowy reset czujnika.

**Składnia:** `SENSor:SOFTReset`  
**Parametry:** brak  
**Czujniki:** TMP117 i SHT45 (obydwa jeśli DUAL)

```python
inst.write('SENSor:SOFTReset')
```

---

### `SENSor:HEATer`

Aktywuje wbudowaną grzałkę SHT45 (np. do usunięcia kondensacji).

**Składnia:** `SENSor:HEATer`  
**Parametry:** brak  
**Czujnik:** SHT45 tylko (dla TMP117: błąd `-221`)  
**Domyślny tryb:** 20 mW przez 1 sekundę

```python
inst.write('SENSor:HEATer')
```

!!! info
    Grzałka uruchamia się na 1 sekundę z mocą 20 mW (tryb `SHT45_HEATER_20MW_1S`, kod `0x1E`). Po jej zakończeniu automatycznie wyłącza się. Wyniki pomiarów podczas działania grzałki mogą być zawyżone — należy odczekać kilka sekund.

---

## Komendy specyficzne dla TMP117

### `SENSor:ALERt:HIGH?` / `SENSor:ALERt:HIGH`

Odczytuje lub ustawia górny próg alertu temperatury TMP117.

**Składnia odczytu:** `SENSor:ALERt:HIGH?`  
**Składnia zapisu:** `SENSor:ALERt:HIGH <temp>`  
**Parametr:** float — temperatura w °C  
**Zakres:** -55,0 … +150,0 °C  
**Odpowiedź (odczyt):** float (°C)  
**Czujnik:** TMP117 lub DUAL

```python
print(inst.query('SENSor:ALERt:HIGH?'))   # 80.0
inst.write('SENSor:ALERt:HIGH 80.0')
```

---

### `SENSor:ALERt:LOW?` / `SENSor:ALERt:LOW`

Odczytuje lub ustawia dolny próg alertu temperatury TMP117.

**Składnia odczytu:** `SENSor:ALERt:LOW?`  
**Składnia zapisu:** `SENSor:ALERt:LOW <temp>`  
**Parametr:** float — temperatura w °C  
**Zakres:** -55,0 … +150,0 °C  
**Odpowiedź (odczyt):** float (°C)  
**Czujnik:** TMP117 lub DUAL

```python
inst.write('SENSor:ALERt:LOW -10.0')
print(inst.query('SENSor:ALERt:LOW?'))   # -10.0
```

---

### `SENSor:ALERt:STATus?`

Odczytuje stan alertu temperatury TMP117 na podstawie bitów w rejestrze CONFIG.

**Składnia:** `SENSor:ALERt:STATus?`  
**Parametry:** brak  
**Odpowiedź:** string  
**Czujnik:** TMP117 lub DUAL

| Odpowiedź | Opis | Bit w CONFIG |
|-----------|------|-------------|
| `"HIGH"` | Temperatura ≥ THIGH | bit 15 |
| `"LOW"` | Temperatura ≤ TLOW | bit 14 |
| `"READY"` | Nowy pomiar gotowy (Data Ready) | bit 13 |
| `"NONE"` | Brak aktywnych alertów | — |

```python
status = inst.query('SENSor:ALERt:STATus?')
if status.strip() == '"HIGH"':
    print('ALERT: temperatura powyżej progu!')
```

---

### `SENSor:MODe?` / `SENSor:MODe`

Odczytuje lub ustawia tryb konwersji TMP117.

**Składnia odczytu:** `SENSor:MODe?`  
**Składnia zapisu:** `SENSor:MODe <mode>`  
**Czujnik:** TMP117 lub DUAL

| Parametr | Wartość CONFIG | Opis |
|----------|---------------|------|
| `CONTinuous` | `0x00` | Ciągły pomiar (domyślny) |
| `SHUTdown` | `0x01` | Wyłączony — oszczędzanie energii |
| `ONESHot` | `0x03` | Jeden pomiar, potem shutdown |

```python
print(inst.query('SENSor:MODe?'))       # "CONTINUOUS"
inst.write('SENSor:MODe SHUTdown')
inst.write('SENSor:MODe CONTinuous')
```

---

### `SENSor:CONVrate?` / `SENSor:CONVrate`

Odczytuje lub ustawia czas cyklu konwersji TMP117 (CONV[2:0]).

**Składnia odczytu:** `SENSor:CONVrate?`  
**Składnia zapisu:** `SENSor:CONVrate <rate>`  
**Parametr:** uint 0–7  
**Czujnik:** TMP117 lub DUAL

| Kod (rate) | Czas konwersji | Uwagi |
|-----------|---------------|-------|
| `0` | 15,5 ms | Najkrótszy |
| `1` | 125 ms | |
| `2` | 250 ms | |
| `3` | 500 ms | |
| `4` | 1 s | **Domyślny** |
| `5` | 4 s | |
| `6` | 8 s | |
| `7` | 16 s | Najdłuższy |

```python
print(inst.query('SENSor:CONVrate?'))   # 4  (= 1s)
inst.write('SENSor:CONVrate 0')          # najszybszy tryb
```

---

## Podsumowanie

| Komenda | Parametr | Odpowiedź | TMP117 | SHT45 |
|---------|----------|-----------|--------|-------|
| `SENSor:TYPE?` | — | string | ✓ | ✓ |
| `SENSor:TEMPerature?` | — | float °C | ✓ | ✓ |
| `SENSor:HUMidity?` | — | float %RH / NaN | NaN | ✓ |
| `SENSor:ID?` | — | uint32 | ✓ | ✓ |
| `SENSor:READperiod?` | — | uint ms | ✓ | ✓ |
| `SENSor:READperiod <ms>` | 50–60000 | — | ✓ | ✓ |
| `SENSor:AVErage?` | — | uint | 1/8/32/64 | 1–255 |
| `SENSor:AVErage <n>` | patrz opis | — | 1/8/32/64 | 1–255 |
| `SENSor:PRECision?` | — | string / NaN | NaN | ✓ |
| `SENSor:PRECision <m>` | LOW/MEDIUM/HIGH | — | ✗ | ✓ |
| `SENSor:SOFTReset` | — | — | ✓ | ✓ |
| `SENSor:HEATer` | — | — | ✗ | ✓ |
| `SENSor:ALERt:HIGH?` | — | float °C | ✓ | ✗ |
| `SENSor:ALERt:HIGH <t>` | -55 … +150 | — | ✓ | ✗ |
| `SENSor:ALERt:LOW?` | — | float °C | ✓ | ✗ |
| `SENSor:ALERt:LOW <t>` | -55 … +150 | — | ✓ | ✗ |
| `SENSor:ALERt:STATus?` | — | string | ✓ | ✗ |
| `SENSor:MODe?` | — | string | ✓ | ✗ |
| `SENSor:MODe <m>` | CONT/SHUT/ONES | — | ✓ | ✗ |
| `SENSor:CONVrate?` | — | uint 0–7 | ✓ | ✗ |
| `SENSor:CONVrate <r>` | 0–7 | — | ✓ | ✗ |
