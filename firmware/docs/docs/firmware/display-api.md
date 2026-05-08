# API wyświetlacza (C)

Dokumentacja publicznych funkcji modułów `display` i `dlr2416`.

---

## Typy danych

### `DisplaySource_t` (`display.h`)

Enum źródła danych wyświetlacza:

```c
typedef enum {
    eDisplaySource_Meas = 0,  // Tryb pomiarowy (auto-format temperatury)
    eDisplaySource_Text,      // Tryb tekstowy (tekst z DISPlay:TEXT)
    eDisplaySource_SIZE       // Sentinel — nie używać
} DisplaySource_t;
```

### `DisplayData_t` (`display.h`)

Wewnętrzna struktura stanu wyświetlacza (nie używana bezpośrednio — dostęp przez funkcje API):

```c
typedef struct {
    char measBuffer[8];     // Bufor danych pomiarowych
    char textBuffer[8];     // Bufor tekstu użytkownika

    uint8_t measNewData;    // Flaga nowych danych pomiarowych
    uint8_t textNewData;    // Flaga nowych danych tekstowych

    uint8_t dispBrightness; // Jasność 1–100 (%)
    uint8_t dispActive;     // Stan: 1=włączony, 0=wyłączony

    uint8_t newBrightnessFlag; // 1 = jasność do aktualizacji
    uint8_t newStateFlag;      // 1 = stan do aktualizacji

    DisplaySource_t activeSource; // Aktualnie wybrany tryb
} DisplayData_t;
```

**Wartości domyślne:**
- `measBuffer`: `"--------"`
- `dispBrightness`: `4` (4%)
- `dispActive`: `1` (włączony)
- `activeSource`: `eDisplaySource_Meas`

---

## Funkcje modułu `display.h`

### Inicjalizacja i zadania

```c
void Display_Init(void);
```
Inicjalizuje wyświetlacz DLR2416, ustawia domyślną jasność i stan. Wywołać raz po `MX_TIM3_Init()`.

```c
void Display_task(void);
```
Główne zadanie wyświetlacza — wywołuj w pętli głównej. Wewnętrzny timer 100 ms. Obsługuje:
- Aktualizację jasności i stanu
- Wyświetlanie kolejnych błędów (1500 ms/błąd)
- Odświeżanie aktywnego bufora

### Sterowanie jasnością i stanem

```c
void    Display_SetBrightness(uint8_t percent);
uint8_t Display_GetBrightness(void);
```
Ustawia/pobiera jasność w procentach (1–100).

```c
void    Display_SetState(uint8_t state);
uint8_t Display_GetState(void);
```
Włącza (`1`) lub wyłącza (`0`) wyświetlacz.

```c
void DisplayOff(void);
```
Natychmiastowe wyłączenie wyświetlacza (wywoływane przed wejściem w DFU).

```c
void DisplayClearAll(void);
```
Czyści obie połówki wyświetlacza — zeruje zawartość DLR2416.

### Zarządzanie zawartością

```c
void Display_SetMeasurement(const char *data, size_t len);
```
Ustawia dane pomiarowe (wywoływane przez firmware po każdym pomiarze). `data` to 8-znakowy string (lub krótszy, dopełniony spacjami).

```c
void Display_SetText(const char *data, size_t len);
```
Ustawia tekst użytkownika (z komendy SCPI `DISPlay:TEXT`). `data` to string do 8 znaków.

```c
void Display_SelectSource(DisplaySource_t src);
DisplaySource_t Display_GetSource(void);
```
Ustawia/pobiera aktywne źródło wyświetlacza.

```c
const char* Display_GetText(void);
```
Zwraca wskaźnik do wewnętrznego bufora tekstu (8 znaków, bez null-terminatora).

### Obsługa błędów

```c
void Display_ShowError(int16_t code);
```
Dodaje kod błędu do kolejki błędów wyświetlacza. Format na ekranie: `ERR:XXXX`. Każdy błąd wyświetlany 1500 ms. Kolejka mieści 4 wpisy.

Wywoływana automatycznie przez `SCPI_Error()` callback przy każdym błędzie SCPI.

### Formatowanie temperatury

```c
void ConvertFloatTempToChar(float t, char *buf);
```
Konwertuje float temperatury na 8-bajtowy bufor dla DLR2416:

- `buf[0]` — znak: `' '` lub `'-'` dla ujemnych
- `buf[1..5]` — cyfry i kropka dziesiętna
- `buf[6]` — spacja
- `buf[7]` — `DEGREE_CHAR` (`0x1B`), specjalny znak stopnia DLR2416

**Uwaga: brak litery 'C'** — bufor ma tylko 8 bajtów.

| Wejście | buf[0..7] | Widoczny tekst |
|---------|-----------|----------------|
| `25.47` | `20 32 35 2E 34 37 20 1B` | `" 25.47 °"` |
| `-10.11` | `2D 31 30 2E 31 31 20 1B` | `"-10.11 °"` |
| `3.5` | `20 33 2E 35 30 30 20 1B` | `" 3.500 °"` |

---

## Funkcje modułu `dlr2416.h`

Niskopoziomowy sterownik DLR2416. Używany wewnętrznie przez `display.c`.

### Stałe

```c
#define DISP1       0       // Identyfikator wyświetlacza 1
#define DISP2       1       // Identyfikator wyświetlacza 2
#define DEGREE_CHAR 0x1B   // Specjalny kod znaku stopnia DLR2416

/* Konfiguracja orientacji — w pliku dlr2416.c */
#define SWAP_DISPLAYS 1    // zamień DISP1↔DISP2 (fizyczne rozmieszczenie)
#define REVERSE_CHARS 1    // odwróć kolejność znaków w module: pos = 3-i
```

Przy `SWAP=1, REV=1` znak indeksu `i` w `WriteString8()` trafia do modułu `(i<4 ? DISP2 : DISP1)` na pozycję `3 - (i%4)`.

### Inicjalizacja

```c
void DLR2416_Init(void);
```
Startuje TIM3 PWM, ustawia CCR=600 (> ARR=255 → 100% brightness), wysyła CLR do obu modułów i czyści wyświetlacz.

### Czyszczenie

```c
void DLR2416_Clear(uint8_t disp);
```
Czyści jeden moduł (`DISP1` lub `DISP2`).

```c
void DLR2416_ClearAll(void);
```
Czyści oba moduły.

### Zapis znaków

```c
void DLR2416_WriteChar(uint8_t disp, uint8_t pos, char c);
```
Zapisuje jeden znak na daną pozycję (0–3) w danym module.

| Parametr | Opis |
|----------|------|
| `disp` | `DISP1` (0) lub `DISP2` (1) |
| `pos` | Pozycja znaku: 0–3 |
| `c` | Kod ASCII znaku (lub `DEGREE_CHAR`) |

```c
void DLR2416_WriteString(uint8_t disp, const char *str);
```
Zapisuje string na jeden moduł (do 4 znaków).

```c
void DLR2416_WriteString8(const char *str);
```
Zapisuje 8-znakowy string na oba moduły. Pierwsze 4 znaki → DISP1, kolejne 4 → DISP2.

### Sterowanie jasnością i PWM

```c
void DLR2416_SetBrightness(uint8_t percent);
```
Ustawia jasność 1–100% wg formuły `CCR = (1199 × percent) / 100`.

!!! note "TIM3 ARR = 255"
    Dla `percent ≥ 22`, CCR > 255 = ARR → pin zawsze HIGH (pełna jasność).
    Rzeczywista regulacja PWM działa tylko dla `percent ≤ 21`.

```c
void DLR2416_PWM_Enable(void);
```
Włącza sygnał PWM podświetlenia (pin PC14 jako AF11).

```c
void DLR2416_PWM_Disable(GPIO_PinState state);
```
Wyłącza PWM — pin PC14 w tryb GPIO z zadanym stanem (`GPIO_PIN_SET` lub `GPIO_PIN_RESET`).

---

## Przykłady użycia C

### Wyświetlenie tekstu

```c
Display_SelectSource(eDisplaySource_Text);
Display_SetText("HELLO   ", 8);
```

### Wyświetlenie temperatury (z auto-formatem)

```c
char buf[8];
ConvertFloatTempToChar(g_sensor.fTemp, buf);
Display_SetMeasurement(buf, 8);
Display_SelectSource(eDisplaySource_Meas);
```

### Bezpośredni zapis przez DLR2416

```c
// Wyświetl "ABCDEFGH" bez warstwy display
DLR2416_WriteString8("ABCDEFGH");

// Wyświetl jeden znak na pozycji 2 modułu 1
DLR2416_WriteChar(DISP1, 2, 'X');

// Ustaw 75% jasność
DLR2416_SetBrightness(75);
```

### Wyłączenie wyświetlacza

```c
Display_SetState(0);       // przez API display
// lub bezpośrednio:
DLR2416_PWM_Disable(GPIO_PIN_RESET);
```
