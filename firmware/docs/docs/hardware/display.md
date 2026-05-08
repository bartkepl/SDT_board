# Wyświetlacz DLR2416

SDT Board używa dwóch modułów **DLR2416** tworząc łącznie **8-znakowy wyświetlacz LED dot-matrix**.

---

## Co to jest DLR2416?

**DLR2416** to inteligentny 4-znakowy wyświetlacz LED z wbudowanym generatorem znaków. Każdy moduł zawiera:

- 4 pozycje znakowe (A–D)
- Wyświetlanie ASCII (5×7 pikseli na znak)
- Wewnętrzną pamięć RAM na 4 znaki
- Interfejs równoległy 7-bitowy (D0–D6)
- Wbudowany dekoder adresu pozycji (A0–A1 → 4 pozycje)

Dwa moduły (DISP1 i DISP2) są połączone równolegle na jednej szynie danych, wybierane oddzielnie sygnałami CE (Chip Enable).

---

## Schemat połączenia

```
STM32C071               DISP1 (pozycje 0–3)    DISP2 (pozycje 4–7)
                         ┌──────────────┐        ┌──────────────┐
PA0–PA6  ──── D0–D6  ───►│              │        │              │
PA7      ──── A0     ───►│   DLR2416    │        │   DLR2416    │
PB0      ──── A1     ───►│              │        │              │
PB1 (INV)─── WR     ───►│              │        │              │
PB2 (INV)─── CLR    ───►│              │        │              │
PB3 (INV)─── CE1    ───►│              │        │              │
PB4 (INV)─── CE2    ───►│              │        │              │
PB12(INV)─── CU     ───►│              │        │              │
PB13     ──── CUE   ───►│              │        │              │
             └──────────────┘        └──────────────┘
                                            ▲              ▲
PB5 (INV)─── CE1 ─────────────────────────┘              │
PB6 (INV)─── CE2 ──────────────────────────────────────────┘

PC14     ──── BL (PWM) ──────────── podświetlenie LED (wspólne)
```

---

## Sygnały sterujące

| Pin | Sygnał | Aktywny | Opis |
|-----|--------|---------|------|
| PA0–PA6 | D0–D6 | — | 7-bitowa szyna danych (kod ASCII) |
| PA7 | A0 | — | Adres pozycji bit 0 |
| PB0 | A1 | — | Adres pozycji bit 1 |
| PB1 | WR | LOW | Strob zapisu (zapis przy opadającym zboczu) |
| PB2 | CLR | LOW | Wyczyszczenie pamięci wyświetlacza |
| PB3 | CE1_1 | LOW | Chip Enable 1 — DISP1 |
| PB4 | CE2_1 | LOW | Chip Enable 2 — DISP1 |
| PB5 | CE1_2 | LOW | Chip Enable 1 — DISP2 |
| PB6 | CE2_2 | LOW | Chip Enable 2 — DISP2 |
| PB12 | CU | LOW | Chip Unit: HIGH=dane, LOW=komenda |
| PB13 | CUE | — | Chip Unit Enable — zatrzask komendy |
| PC14 | BL | PWM | Podświetlenie — TIM3 CH2, AF11 |

---

## Adresowanie pozycji

| A1 | A0 | Pozycja (4 znaki na moduł) |
|----|----|---------------------------|
| 0 | 0 | Pozycja 0 (skrajna lewa) |
| 0 | 1 | Pozycja 1 |
| 1 | 0 | Pozycja 2 |
| 1 | 1 | Pozycja 3 (skrajna prawa modułu) |

Dla 8-znakowego wyświetlacza:
- DISP1 obsługuje pozycje 0–3 (lewe 4 znaki)
- DISP2 obsługuje pozycje 4–7 (prawe 4 znaki)

---

## Protokół zapisu znaku

Procedura zapisu jednego znaku:

1. Ustaw adres: A1, A0 = numer pozycji (0–3)
2. Ustaw dane: D0–D6 = kod ASCII znaku
3. Wybierz moduł: CE1=LOW, CE2=LOW (odpowiedni moduł)
4. Strob zapisu: WR=LOW, następnie WR=HIGH

---

## Konfiguracja orientacji: SWAP i REVERSE

W `dlr2416.c` dwie stałe kompilacji wpływają na `DLR2416_WriteString8()`:

```c
#define SWAP_DISPLAYS 1   // zamień DISP1↔DISP2 (domyślnie: 1)
#define REVERSE_CHARS 1   // odwróć kolejność znaków w module pos=3-i (domyślnie: 1)
```

| Stała | Efekt |
|-------|-------|
| `SWAP_DISPLAYS 1` | Znaki 0–3 idą do DISP2, znaki 4–7 do DISP1 |
| `REVERSE_CHARS 1` | Pozycja i → 3-i (odwrócona kolejność wewnątrz modułu) |

Kombinacja obu pozwala dopasować wyświetlanie do fizycznej orientacji modułów na płytce bez zmiany okablowania.

---

## Sterowanie jasnością (PWM)

Jasność sterowana jest sygnałem **PWM** przez **TIM3 Channel 2** na pinie **PC14** (AF11).

| Parametr | Wartość |
|----------|---------|
| TIM3 ARR (Period) | **255** (8-bit, skonfigurowane w CubeMX) |
| Formuła CCR | `(1199 × percent) / 100` |
| CCR przy 1% | 11 |
| CCR przy 21% | 251 (~= ARR) |
| CCR przy ≥22% | > 255 → pin zawsze HIGH (100% brightness) |
| Domyślny CCR (init) | 600 → zawsze HIGH (pełna jasność po starcie) |
| Wyłączony | `DLR2416_PWM_Disable(GPIO_PIN_RESET)` — pin jako GPIO LOW |

```c
/* Kod w DLR2416_SetBrightness() */
uint16_t value = (1199 * percent) / 100;
__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, value);
```

!!! warning "Efektywna regulacja tylko ≤21%"
    TIM3 ARR = 255. Dla brightness ≥ 22%, CCR przekracza ARR i pin pozostaje
    ciągle HIGH (100% jasność). Regulacja PWM działa tylko dla wartości 1–21%.
    Powyżej efekt jest identyczny — maksymalna jasność.

---

## Znak stopnia

Wyświetlacz nie obsługuje standardowego znaku stopnia (°, ASCII 0xB0). Zamiast tego DLR2416 używa własnego kodu:

```c
#define DEGREE_CHAR 0x1B   /* Specjalny znak stopnia DLR2416 */
```

Firmware używa go automatycznie podczas formatowania temperatury w trybie pomiarowym.

---

## Format wyświetlania temperatury

W trybie `SOURce 0` (pomiarowy) firmware formatuje temperaturę przez `ConvertFloatTempToChar()`.

Format: dokładnie **8 bajtów**. Ostatni bajt to `DEGREE_CHAR` = `0x1B`. **Brak litery 'C'**.

```
Mapa pozycji: [0]  [1][2][3][4][5]  [6]  [7]
               sign  cyfry/kropka    spc   °

Przykłady (hex + ASCII):
 25.47°  →  20 32 35 2E 34 37 20 1B  = " 25.47 °"
-10.11°  →  2D 31 30 2E 31 31 20 1B  = "-10.11 °"
 3.500°  →  20 33 2E 35 30 30 20 1B  = " 3.500 °"
```

Zakres:
- `|T| < 10` → format `±D.DDD` (3 miejsca po przecinku)
- `|T| ≥ 10` → format `±DD.DD` (2 miejsca po przecinku)

---

## Komunikaty błędów

Gdy wystąpi błąd czujnika, wyświetlacz pokazuje:

```
ERR:-241
ERR:-240
ERR:-230
```

- Format: `ERR:XXXX` (8 znaków)
- Czas wyświetlania: **1500 ms** na błąd
- Kolejka: max **4 błędy**
- Po wyczyszczeniu: powrót do poprzedniego widoku
