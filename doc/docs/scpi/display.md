# DISPlay — Sterowanie wyświetlaczem

Komendy `DISPlay:` pozwalają sterować 8-znakowym wyświetlaczem LED DLR2416: jasnością, stanem, źródłem danych i własnym tekstem.

---

## `DISPlay:BRIGhtness?` / `DISPlay:BRIGhtness`

Odczytuje lub ustawia jasność wyświetlacza.

**Składnia odczytu:** `DISPlay:BRIGhtness?`  
**Składnia zapisu:** `DISPlay:BRIGhtness <percent>`  
**Parametr:** uint — jasność w procentach  
**Zakres:** 1 … 100 (%)  
**Wartość domyślna:** 20 (%)  
**Odpowiedź (odczyt):** uint (%)

```python
print(inst.query('DISPlay:BRIGhtness?'))   # 20
inst.write('DISPlay:BRIGhtness 50')         # 50% jasności
inst.write('DISPlay:BRIGhtness 100')        # maksymalna jasność
```

!!! info "Implementacja PWM"
    Jasność sterowana przez TIM3 CH2 (PC14, AF11). Formuła: `CCR = (256 × percent) / 100`.  
    TIM3 ARR = 255, więc CCR > 255 → 100%.

---

## `DISPlay:STATe?` / `DISPlay:STATe`

Odczytuje lub ustawia stan wyświetlacza (włączony / wyłączony).

**Składnia odczytu:** `DISPlay:STATe?`  
**Składnia zapisu:** `DISPlay:STATe <state>`  
**Parametr:** `0`, `1`, `OFF`, `ON`  
**Odpowiedź (odczyt):** `0` (wyłączony) lub `1` (włączony)

```python
print(inst.query('DISPlay:STATe?'))   # 1

inst.write('DISPlay:STATe OFF')        # wyłącz
inst.write('DISPlay:STATe ON')         # włącz
inst.write('DISPlay:STATe 0')          # wyłącz (alternatywna forma)
inst.write('DISPlay:STATe 1')          # włącz
```

!!! note
    Wyłączenie wyświetlacza (`STATe 0`) odcina sygnał PWM — wyświetlacz gaśnie. Dane są w pamięci i wrócą po włączeniu.

---

## `DISPlay:SOURce?` / `DISPlay:SOURce`

Odczytuje lub ustawia źródło danych wyświetlacza.

**Składnia odczytu:** `DISPlay:SOURce?`  
**Składnia zapisu:** `DISPlay:SOURce <source>`  
**Odpowiedź (odczyt):** `0` lub `1`

| Wartość | Stała (`DisplaySource_t`) | Opis |
|---------|--------------------------|------|
| `0` | `eDisplaySource_Meas` | Wyświetla automatycznie sformatowane pomiary |
| `1` | `eDisplaySource_Text` | Wyświetla tekst ustawiony przez `DISPlay:TEXT` |

```python
inst.write('DISPlay:SOURce 0')   # tryb pomiarowy (domyślny)
inst.write('DISPlay:SOURce 1')   # tryb tekstowy

print(inst.query('DISPlay:SOURce?'))   # 1
```

---

## `DISPlay:TEXT?` / `DISPlay:TEXT`

Odczytuje lub ustawia tekst wyświetlany na wyświetlaczu (w trybie tekstowym).

**Składnia odczytu:** `DISPlay:TEXT?`  
**Składnia zapisu:** `DISPlay:TEXT "<text>"`  
**Parametr:** string — maksymalnie **8 znaków**  
**Odpowiedź (odczyt):** string (8 znaków, uzupełniony spacjami)

```python
# Ustaw tekst i przełącz w tryb tekstowy
inst.write('DISPlay:SOURce 1')
inst.write('DISPlay:TEXT "HELLO   "')

# Odczyt aktualnego tekstu
text = inst.query('DISPlay:TEXT?')
print(repr(text))   # "HELLO   "
```

!!! warning "Długość tekstu"
    Bufor tekstowy ma 8 znaków. Jeśli podasz krótszy tekst, pozostałe pozycje zostaną wypełnione spacjami. Tekst dłuższy niż 8 znaków zostanie obcięty.

!!! tip "Znak stopnia"
    Wyświetlacz obsługuje specjalny znak stopnia (°) zakodowany jako bajt `0x1B`. Nie da się go przesłać bezpośrednio przez SCPI jako tekst ASCII — jest używany wewnętrznie przez tryb pomiarowy.

---

## Tryb pomiarowy — format wyświetlania

W trybie `SOURce 0` firmware automatycznie formatuje temperaturę na wyświetlacz za pomocą `ConvertFloatTempToChar()`.

Format: dokładnie **8 znaków**. Ostatni znak to `DEGREE_CHAR` (`0x1B`) — specjalny kod stopnia DLR2416. **Nie ma litery 'C'** — wyświetlacz ma tylko 8 pozycji znakowych.

```
Pozycje: [0]  [1][2][3][4][5]  [6][7]
          sign  cyfry/kropka   spc  °

Przykłady:
 25.47 °  →  ' ','2','5','.','4','7',' ',0x1B
-10.11 °  →  '-','1','0','.','1','1',' ',0x1B
 3.500 °  →  ' ','3','.','5','0','0',' ',0x1B
```

- T ∈ [0..9]: format `S D.DDD` (3 miejsca po przecinku)
- T ∈ [10..99]: format `S DD.DD` (2 miejsca po przecinku)
- Wartości ujemne: `'-'` na pozycji [0]

---

## Wyświetlanie błędów

Gdy firmware wykryje błąd czujnika, automatycznie wyświetla go na ekranie w formacie:

```
ERR:-241
```

Każdy błąd jest wyświetlany przez **1,5 sekundy**, a następnie wraca poprzedni widok. Kolejka błędów wyświetlacza mieści maksymalnie **4 wpisy**.

---

## Przykłady

### Komunikat powitalny

```python
inst.write('DISPlay:BRIGhtness 80')
inst.write('DISPlay:SOURce 1')
inst.write('DISPlay:TEXT "SDT INIT"')
time.sleep(2)
inst.write('DISPlay:SOURce 0')   # wróć do pomiarów
```

### Wyłączenie wyświetlacza w nocy

```python
import datetime

hour = datetime.datetime.now().hour
if 23 <= hour or hour < 7:
    inst.write('DISPlay:STATe OFF')
else:
    inst.write('DISPlay:STATe ON')
    inst.write('DISPlay:BRIGhtness 30')
```

---

## Podsumowanie

| Komenda | Parametr | Odpowiedź | Opis |
|---------|----------|-----------|------|
| `DISPlay:BRIGhtness?` | — | uint 1–100 | Odczyt jasności |
| `DISPlay:BRIGhtness <n>` | 1–100 | — | Ustaw jasność (%) |
| `DISPlay:STATe?` | — | `0` lub `1` | Odczyt stanu |
| `DISPlay:STATe <s>` | 0/1/ON/OFF | — | Włącz/wyłącz |
| `DISPlay:SOURce?` | — | `0` lub `1` | Odczyt źródła |
| `DISPlay:SOURce <s>` | 0/1 | — | Pomiar lub tekst |
| `DISPlay:TEXT?` | — | string 8 znaków | Odczyt tekstu |
| `DISPlay:TEXT "<s>"` | string ≤8 znaków | — | Ustaw tekst |
