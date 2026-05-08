# IEEE 488.2 — Komendy mandatoryjne

Zestaw 13 komend wymaganych przez standard SCPI V1999.0 sekcja 4.1.1. Wszystkie urządzenia SCPI muszą je implementować.

---

## `*CLS` — Clear Status

Czyści rejestry statusu: ESR (Event Status Register) i opróżnia kolejkę błędów.

**Składnia:** `*CLS`  
**Parametry:** brak  
**Odpowiedź:** brak

```python
inst.write('*CLS')
```

---

## `*ESE` / `*ESE?` — Event Status Enable

Ustawia lub odczytuje maskę rejestru Event Status Enable (ESE).

**Składnia zapisu:** `*ESE <value>`  
**Składnia odczytu:** `*ESE?`  
**Parametry:** `<value>` — liczba całkowita 0–255 (maska bitowa)  
**Odpowiedź:** liczba całkowita (aktualna wartość ESE)

```python
inst.write('*ESE 32')        # ustaw bit PON
print(inst.query('*ESE?'))   # odczyt: 32
```

---

## `*ESR?` — Event Status Register

Odczytuje i zeruje rejestr Event Status Register (ESR).

**Składnia:** `*ESR?`  
**Parametry:** brak  
**Odpowiedź:** liczba całkowita 0–255

```python
esr = int(inst.query('*ESR?'))
```

---

## `*IDN?` — Identification Query

Zwraca ciąg identyfikacyjny urządzenia w formacie SCPI.

**Składnia:** `*IDN?`  
**Parametry:** brak  
**Odpowiedź:** `<producent>,<model>,<numer_seryjny>,<wersja_firmware>`

| Pole | Wartość |
|------|---------|
| Producent | `bartkepl` |
| Model | `SDT` |
| Numer seryjny | 8-znakowy hex (FNV-1a hash z DEVID+REVID+UID MCU) |
| Wersja firmware | `1.0` |

```python
print(inst.query('*IDN?'))
# bartkepl,SDT,A1B2C3D4,1.0
```

---

## `*OPC` / `*OPC?` — Operation Complete

`*OPC` ustawia bit OPC w ESR po zakończeniu wszystkich operacji.  
`*OPC?` blokuje odpowiedź do momentu zakończenia wszystkich operacji, a następnie zwraca `1`.

**Składnia:** `*OPC` lub `*OPC?`  
**Parametry:** brak  
**Odpowiedź** (`*OPC?`): `1`

```python
inst.write('*OPC')
result = inst.query('*OPC?')   # zwraca "1"
```

---

## `*RST` — Reset

Wykonuje reset urządzenia do stanu domyślnego. Inicjalizuje ponownie wszystkie moduły.

**Składnia:** `*RST`  
**Parametry:** brak  
**Odpowiedź:** brak

```python
inst.write('*RST')
```

!!! note
    Odpowiednik `SYSTem:RST`. Wykonuje reset programowy MCU.

---

## `*SRE` / `*SRE?` — Service Request Enable

Ustawia lub odczytuje maskę rejestru Service Request Enable (SRE).

**Składnia zapisu:** `*SRE <value>`  
**Składnia odczytu:** `*SRE?`  
**Parametry:** `<value>` — liczba całkowita 0–255  
**Odpowiedź:** liczba całkowita

```python
inst.write('*SRE 16')
print(inst.query('*SRE?'))   # 16
```

---

## `*STB?` — Status Byte

Odczytuje Status Byte Register.

**Składnia:** `*STB?`  
**Parametry:** brak  
**Odpowiedź:** liczba całkowita 0–255

```python
stb = int(inst.query('*STB?'))
```

---

## `*TST?` — Self-Test

Wykonuje autotest urządzenia i zwraca wynik.

**Składnia:** `*TST?`  
**Parametry:** brak  
**Odpowiedź:** `0` — brak błędów, `1` — błąd

| Wartość | Znaczenie |
|---------|-----------|
| `0` | Urządzenie sprawne (czujnik wykryty i działający) |
| `1` | Błąd — czujnik nie wykryty lub w stanie błędu |

```python
result = int(inst.query('*TST?'))
if result != 0:
    print('Autotest: BŁĄD — sprawdź czujnik!')
```

!!! info "Implementacja"
    Autotest sprawdza, czy `g_sensor.type != SENSOR_NONE && g_sensor.type != SENSOR_ERROR`. W przypadku błędu dodaje `-241` (Hardware missing) do kolejki błędów SCPI.

---

## `*WAI` — Wait to Continue

Wstrzymuje dalsze wykonywanie komend do zakończenia wszystkich oczekujących operacji.

**Składnia:** `*WAI`  
**Parametry:** brak  
**Odpowiedź:** brak

```python
inst.write('*WAI')
```

---

## Podsumowanie

| Komenda | Parametr | Odpowiedź | Opis |
|---------|----------|-----------|------|
| `*CLS` | — | — | Wyczyść rejestry statusu |
| `*ESE <n>` | int 0–255 | — | Ustaw ESE |
| `*ESE?` | — | int | Odczyt ESE |
| `*ESR?` | — | int | Odczyt i zerowanie ESR |
| `*IDN?` | — | string | Identyfikacja |
| `*OPC` | — | — | Operation Complete (bit) |
| `*OPC?` | — | `1` | Czekaj na zakończenie |
| `*RST` | — | — | Reset urządzenia |
| `*SRE <n>` | int 0–255 | — | Ustaw SRE |
| `*SRE?` | — | int | Odczyt SRE |
| `*STB?` | — | int | Status Byte |
| `*TST?` | — | `0` lub `1` | Autotest |
| `*WAI` | — | — | Czekaj na zakończenie |
