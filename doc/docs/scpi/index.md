# Komendy SCPI — Przegląd

SDT Board implementuje protokół **SCPI** (Standard Commands for Programmable Instruments) w wersji 1999.0, zgodny z IEEE 488.2, przesyłany przez interfejs **USB-TMC** (USB Test and Measurement Class).

---

## Grupy komend

| Grupa | Prefiks | Opis |
|-------|---------|------|
| [IEEE 488.2](ieee488.md) | `*` | Standardowe komendy mandatoryjne |
| [SYSTem](system.md) | `SYST:` | Zarządzanie systemem, błędy, reset, bootloader |
| [SENSor](sensor.md) | `SENS:` | Czujniki — temperatura, wilgotność, konfiguracja |
| [DISPlay](display.md) | `DISP:` | Sterowanie wyświetlaczem LED |

---

## Konwencje składni SCPI

### Skróty komend

SCPI pozwala używać skróconych form komend — litery pisane wielkimi literami tworzą obowiązkowe minimum:

| Pełna forma | Skrót | Opis |
|-------------|-------|------|
| `SENSor:TEMPerature?` | `SENS:TEMP?` | Odczyt temperatury |
| `DISPlay:BRIGhtness` | `DISP:BRIG` | Jasność wyświetlacza |
| `SYSTem:ERRor:NEXT?` | `SYST:ERR:NEXT?` | Następny błąd |

### Parametry opcjonalne

Parametry w nawiasach kwadratowych `[...]` są opcjonalne:

```
SYSTem:ERRor[:NEXT]?   →  SYSTem:ERRor?  lub  SYSTem:ERRor:NEXT?
SYSTem:ID? [SHORT|LONG]  →  SYSTem:ID?  lub  SYSTem:ID? LONG
```

### Typy wartości

| Typ | Przykład | Opis |
|-----|---------|------|
| `<bool>` | `0`, `1`, `ON`, `OFF` | Wartość logiczna |
| `<uint>` | `50`, `100` | Liczba całkowita nieujemna |
| `<float>` | `23.5`, `-10.0` | Liczba zmiennoprzecinkowa |
| `<string>` | `"HELLO   "` | Ciąg znaków w cudzysłowie |
| `<choice>` | `HIGH`, `LOW` | Wybór ze zdefiniowanej listy |

---

## Kolejka błędów SCPI

Urządzenie posiada kolejkę błędów (rozmiar: 16) zgodną ze standardem SCPI. Każda nieprawidłowa operacja dodaje do niej wpis.

```python
# Odczyt liczby błędów
count = inst.query('SYSTem:ERRor:COUNt?')

# Odczyt kolejnych błędów
err = inst.query('SYSTem:ERRor:NEXT?')
# Format: <kod>,"<opis>"
# Przykład: -222,"Data out of range"

# Wyczyszczenie rejestru statusu (nie kolejki!)
inst.write('*CLS')
```

Szczegółowa lista kodów błędów: [SYSTem — Kody błędów](system.md#kody-bledow-scpi).

---

## Wymagania sprzętowe a dostępność komend

Niektóre komendy są dostępne tylko dla konkretnych czujników:

| Komenda | TMP117 | SHT45 | DUAL |
|---------|--------|-------|------|
| `SENSor:TEMPerature?` | ✓ | ✓ | ✓ |
| `SENSor:HUMidity?` | NaN | ✓ | ✓ |
| `SENSor:ALERt:*` | ✓ | ✗ | ✓ |
| `SENSor:MODe` | ✓ | ✗ | ✓ |
| `SENSor:CONVrate` | ✓ | ✗ | ✓ |
| `SENSor:PRECision` | ✗ | ✓ | ✓ |
| `SENSor:HEATer` | ✗ | ✓ | ✓ |
| `SENSor:AVErage` | 1/8/32/64 | 1–255 | ✓ |
