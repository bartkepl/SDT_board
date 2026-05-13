# SYSTem — Komendy systemowe

Komendy `SYSTem:` służą do zarządzania urządzeniem: obsługi błędów, wersjonowania, identyfikacji MCU, restartu i wejścia w tryb aktualizacji firmware.

---

## `SYSTem:ERRor[:NEXT]?`

Odczytuje i usuwa z kolejki następny błąd SCPI.

**Składnia:** `SYSTem:ERRor?` lub `SYSTem:ERRor:NEXT?`  
**Parametry:** brak  
**Odpowiedź:** `<kod>,"<opis>"`

| Wynik | Znaczenie |
|-------|-----------|
| `0,"No error"` | Kolejka błędów jest pusta |
| `-222,"Data out of range"` | Parametr poza dopuszczalnym zakresem |
| `-241,"Hardware missing"` | Czujnik nie wykryty |

```python
err = inst.query('SYSTem:ERRor:NEXT?')
print(err)   # -241,"Hardware missing"
```

!!! tip
    Odczytuj błędy w pętli, dopóki wartość kodu != 0:
    ```python
    while True:
        err = inst.query('SYSTem:ERRor?')
        code = int(err.split(',')[0])
        if code == 0:
            break
        print(f'Błąd: {err}')
    ```

---

## `SYSTem:ERRor:COUNt?`

Zwraca liczbę błędów aktualnie w kolejce.

**Składnia:** `SYSTem:ERRor:COUNt?`  
**Parametry:** brak  
**Odpowiedź:** liczba całkowita ≥ 0

```python
count = int(inst.query('SYSTem:ERRor:COUNt?'))
print(f'Błędów w kolejce: {count}')
```

---

## `SYSTem:VERSion?`

Zwraca numer wersji standardu SCPI zaimplementowanego w urządzeniu.

**Składnia:** `SYSTem:VERSion?`  
**Parametry:** brak  
**Odpowiedź:** `1999` (odpowiada wersji SCPI 1999.0)

```python
print(inst.query('SYSTem:VERSion?'))   # 1999
```

---

## `SYSTem:ID?`

Zwraca numer seryjny MCU.

**Składnia:** `SYSTem:ID? [SHORT|LONG]`  
**Parametry:** opcjonalny wybór formatu

| Parametr | Format | Długość | Źródło danych |
|----------|--------|---------|---------------|
| `SHORT` (domyślny) | FNV-1a hash 20 bajtów → hex | **8 znaków** | DEVID + REVID + UID 96-bit |
| `LONG` | 20 bajtów danych MCU → hex | **40 znaków** | DEVID + REVID + UID 96-bit |

!!! info "Skład 20 bajtów"
    `HAL_GetDEVID()` (4B) + `HAL_GetREVID()` (4B) + `UID_Word0` (4B) + `UID_Word1` (4B) + `UID_Word2` (4B)

    SHORT to 8-znakowy hash FNV-1a — unikalny per układ, ale nie odwracalny.  
    LONG to 40-znakowy hex dump tych samych 20 bajtów.

```python
print(inst.query('SYSTem:ID?'))          # np. A1B2C3D4  (8 znaków, domyślny SHORT)
print(inst.query('SYSTem:ID? SHORT'))    # A1B2C3D4
print(inst.query('SYSTem:ID? LONG'))     # pełne 40 znaków hex
```

---

## `SYSTem:BOOTloader:ENter`

Wchodzi w tryb USB DFU (Device Firmware Upgrade) w celu aktualizacji firmware.

**Składnia:** `SYSTem:BOOTloader:ENter`  
**Parametry:** brak  
**Odpowiedź:** brak (urządzenie natychmiast restartuje)

```python
inst.write('SYSTem:BOOTloader:ENter')
```

!!! warning "Działanie"
    Komenda:

    1. Wyłącza wyświetlacz
    2. Wyłącza wszystkie przerwania i SysTick
    3. Deinicjalizuje RCC
    4. Przekazuje sterowanie do ROM bootloadera STM32 pod adresem `0x1FFF0000`
    
    Po tej komendzie urządzenie znika z listy portów SCPI i pojawia się jako urządzenie DFU. Użyj **STM32CubeProgrammer** lub `dfu-util` do wgrania nowego firmware.

---

## `SYSTem:RST`

Wykonuje programowy reset MCU.

**Składnia:** `SYSTem:RST`  
**Parametry:** brak  
**Odpowiedź:** brak

```python
inst.write('SYSTem:RST')
```

!!! note
    Odpowiednik komendy `*RST`. Wywołuje `HAL_NVIC_SystemReset()`.

---

## Kody błędów SCPI {#kody-bledow-scpi}

Tabela błędów generowanych przez SDT Board:

### Błędy urządzenia (Device-specific errors)

| Kod | Opis | Przyczyna |
|-----|------|-----------|
| `-230` | Data corrupt or stale | Błąd CRC w danych z SHT45 |
| `-240` | Hardware error | Błąd komunikacji I2C z TMP117 |
| `-241` | Hardware missing | Czujnik nie wykryty podczas startu |
| `-365` | Time out error | Timeout DMA lub I2C |

### Błędy wykonania (Execution errors)

| Kod | Opis | Przyczyna |
|-----|------|-----------|
| `-200` | Execution error | Ogólny błąd wykonania |
| `-220` | Parameter error | Zły format parametru |
| `-221` | Settings conflict | Komenda nieobsługiwana przez aktywny czujnik |
| `-222` | Data out of range | Parametr poza dopuszczalnym zakresem |
| `-223` | Too much data | Zbyt dużo danych |
| `-224` | Illegal parameter value | Niedozwolona wartość parametru |
| `-225` | Out of memory | Brak pamięci |

### Błędy składni (Command errors)

| Kod | Opis |
|-----|------|
| `-100` | Command error |
| `-102` | Syntax error |
| `-103` | Invalid separator |
| `-104` | Data type error |
| `-108` | Parameter not allowed |
| `-109` | Missing parameter |
| `-113` | Undefined header |

### Błędy danych pytania (Query errors)

| Kod | Opis |
|-----|------|
| `-410` | Query INTERRUPTED |
| `-420` | Query UNTERMINATED |
| `-430` | Query DEADLOCKED |

---

## Podsumowanie

| Komenda | Parametr | Odpowiedź | Opis |
|---------|----------|-----------|------|
| `SYSTem:ERRor[:NEXT]?` | — | `<kod>,"<opis>"` | Następny błąd z kolejki |
| `SYSTem:ERRor:COUNt?` | — | int ≥ 0 | Liczba błędów w kolejce |
| `SYSTem:VERSion?` | — | `1999` | Wersja standardu SCPI |
| `SYSTem:ID? [SHORT\|LONG]` | `SHORT`/`LONG` | string 8 lub 40 znaków | Numer seryjny MCU |
| `SYSTem:BOOTloader:ENter` | — | — | Wejście w tryb DFU |
| `SYSTem:RST` | — | — | Programowy reset MCU |
