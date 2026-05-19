# CALibration — Kalibracja wielomianowa

Subsystem `CALibration:` umożliwia korekcję błędów systematycznych czujnika temperatury za pomocą wielomianu stopnia 1–5. Współczynniki są przechowywane w pamięci flash i przeżywają wyłączenie zasilania.

---

## Zasada działania

Po włączeniu kalibracji każdy odczyt temperatury jest korygowany przed przekazaniem do SCPI i wyświetlacza:

```
T_kal = a0 + a1·T + a2·T² + a3·T³ + a4·T⁴ + a5·T⁵
```

Współczynniki domyślne (brak korekcji):

| Współczynnik | Wartość domyślna |
|---|---|
| a0 | 0.0 |
| a1 | 1.0 (mnożnik liniowy) |
| a2 … a5 | 0.0 |

Kalibracja jest **nieaktywna** po wyjściu fabrycznym — należy ją jawnie włączyć komendą `CAL:STAT ON` po wysłaniu współczynników.

---

## Proces kalibracji — krok po kroku

### 1. Zebranie punktów pomiarowych

Umieść urządzenie obok wzorca temperatury (kalibrowanego termometru lub punktu lodowego/wrzącego). Dla każdej temperatury zapisz parę:

- **T_measured** — odczyt z `SENSor:TEMPerature?`
- **T_reference** — wskazanie wzorca

Zalecana liczba punktów: **≥ 5** na zakres kalibracji. Punkty powinny być równomiernie rozłożone po zakresie.

!!! tip "Zalecany zakres"
    Czujniki TMP117 i SHT45 mają specyfikację do –40 … +125 °C.
    Praktyczna kalibracja w zakresie **0 … 80 °C** z krokiem co 10–20 °C jest wystarczająca dla większości zastosowań.

### 2. Dopasowanie wielomianu

Najwygodniej używać zakładki **Calibration** w aplikacji SDT Board Companion:

1. Wpisz pary (T_measured, T_reference) w tabelce
2. Wybierz stopień wielomianu (zalecane: **2 lub 3** — wyższe stopnie mogą dawać oscylacje poza zakresem danych)
3. Kliknij **Fit polynomial**
4. Sprawdź wykres residuów — powinny być symetryczne i losowe

Alternatywnie, ręcznie w Pythonie:

```python
import numpy as np

t_meas = [0.22, 10.38, 19.87, 30.45, 40.19, 49.93, 60.21]
t_ref  = [0.00, 10.00, 20.00, 30.00, 40.00, 50.00, 60.00]

# Stopień 2 — dopasowanie T_ref = f(T_meas)
coeffs = np.polyfit(t_meas, t_ref, deg=2)[::-1]  # [a0, a1, a2]
# Uzupełnij do 6 wartości
a0, a1, a2 = coeffs
a3 = a4 = a5 = 0.0
```

### 3. Wysłanie współczynników do urządzenia

```python
# Wyślij 6 współczynników (niewykorzystane wyższe stopnie = 0)
inst.write(f"CAL:COEF {a0:.8e},{a1:.8e},{a2:.8e},{a3:.8e},{a4:.8e},{a5:.8e}")

# Ustaw datę kalibracji (YYYY-MM-DD)
inst.write('CAL:DATE "2026-05-20"')

# Włącz korekcję
inst.write("CAL:STAT ON")

# Zapisz do pamięci flash
inst.write("SYST:CONF:SAVE")
```

### 4. Weryfikacja

```python
# Odczytaj skalibrowaną temperaturę
t_cal = float(inst.query("SENSor:TEMPerature?"))

# Odczytaj aktualny stan
print(inst.query("CAL:STAT?"))   # 1
print(inst.query("CAL:DATE?"))   # 2026-05-20
print(inst.query("CAL:COEF?"))   # a0,a1,a2,a3,a4,a5
```

---

## Komendy

### `CALibration:COEFficient`

Ustawia 6 współczynników wielomianu kalibracyjnego.

**Składnia:** `CAL:COEF <a0>,<a1>,<a2>,<a3>,<a4>,<a5>`  
**Parametry:** 6 liczb zmiennoprzecinkowych (scientific notation lub decimal)

```python
inst.write("CAL:COEF 0.15e+00,9.98e-01,-1.2e-04,0,0,0")
```

---

### `CALibration:COEFficient?`

Odczytuje aktualnie zapisane współczynniki.

**Składnia:** `CAL:COEF?`  
**Odpowiedź:** 6 liczb oddzielonych przecinkami

```python
resp = inst.query("CAL:COEF?")
# np. "1.500000e-01,9.980000e-01,-1.200000e-04,0.000000e+00,0.000000e+00,0.000000e+00"
coeffs = [float(x) for x in resp.split(",")]
```

---

### `CALibration:STATe`

Włącza lub wyłącza stosowanie korekcji kalibracyjnej.

**Składnia:** `CAL:STAT <ON|OFF|1|0>`

```python
inst.write("CAL:STAT ON")   # włącz
inst.write("CAL:STAT OFF")  # wyłącz (dane surowe bez korekcji)
```

!!! warning "Pamiętaj o zapisie"
    Zmiana stanu kalibracji jest ulotna do czasu wywołania `SYST:CONF:SAVE`.

---

### `CALibration:STATe?`

Odczytuje stan kalibracji.

**Odpowiedź:** `1` (aktywna) lub `0` (nieaktywna)

---

### `CALibration:DATE`

Zapisuje datę kalibracji w formacie `YYYY-MM-DD` (10 znaków).

**Składnia:** `CAL:DATE <"YYYY-MM-DD">`

```python
inst.write('CAL:DATE "2026-05-20"')
```

---

### `CALibration:DATE?`

Odczytuje zapisaną datę kalibracji.

**Odpowiedź:** string `"YYYY-MM-DD"` lub `"----------"` jeśli nie ustawiono.

---

### `CALibration:RESet`

Przywraca współczynniki kalibracyjne do wartości identycznościowych (brak korekcji) i wyłącza kalibrację.

**Składnia:** `CAL:RES`  
**Efekt:** a0=0, a1=1, a2..a5=0, cal_active=0, data="----------"

```python
inst.write("CAL:RES")
inst.write("SYST:CONF:SAVE")  # utrwal reset w flash
```

---

## Wybór stopnia wielomianu

| Stopień | Parametry | Kiedy stosować |
|---------|-----------|----------------|
| 1 | a0, a1 | Stały offset + skala — czujnik z małą nieliniowością |
| 2 | + a2 | Typowa kalibracja w zakresie 0–80 °C |
| 3 | + a3 | Szeroki zakres z wyraźną krzywizną |
| 4–5 | + a4, a5 | Tylko gdy dane na to wskazują — ryzyko przepasowania |

!!! danger "Przepasowanie (overfitting)"
    Stopień ≥ liczby punktów − 1 daje zerowe residua na punktach kalibracyjnych, ale może dawać duże błędy między punktami. Zawsze weryfikuj kalibrację na punktach **niebędących** w zestawie kalibracyjnym.
