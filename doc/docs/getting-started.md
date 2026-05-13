# Pierwsze kroki

## Wymagania

- Kabel USB-C
- System Windows / Linux / macOS z obsługą USB-TMC
- Opcjonalnie: Python 3.8+ z biblioteką PyVISA lub NI-VISA

---

## Podłączenie urządzenia

1. Podłącz SDT Board do komputera kablem USB-C.
2. Urządzenie zostanie wykryte jako **USB Test and Measurement Class** (USB-TMC).
3. Na wyświetlaczu pojawi się temperatura — oznacza to pomyślne uruchomienie.

!!! note "Sterowniki"
    Na systemie **Windows** może być wymagane zainstalowanie sterownika USBTMC. Polecane opcje:
    
    - **NI-VISA** (National Instruments) — pełna obsługa, instalator dostępny na stronie NI.
    - **PyVISA-py** — sterownik pure-Python, nie wymaga NI-VISA.

---

## Python — PyVISA

### Instalacja

```bash
pip install pyvisa pyvisa-py
```

### Wykrycie urządzenia

```python
import pyvisa

rm = pyvisa.ResourceManager('@py')   # użyj '@py' jeśli nie masz NI-VISA
resources = rm.list_resources()
print(resources)
# Przykład: ('USB0::0xCAFE::0x4000::SDT_A1B2C3D4::INSTR',)
```

!!! info "VID/PID i format numeru seryjnego USB"
    SDT Board używa **VID = `0xCAFE`**, **PID = `0x4000`** (TinyUSB domyślne).  
    Numer seryjny USB ma format **`SDT_<8znakówHash>`**, gdzie hash to FNV-1a z danych MCU (DEVID + REVID + UID 96-bit).  
    Przykład: `SDT_A1B2C3D4`. Wpisz dokładnie tę wartość zamiast `SDT_YOURHASH`.

### Podstawowe komendy

```python
import pyvisa

rm = pyvisa.ResourceManager('@py')
inst = rm.open_resource('USB0::0xCAFE::0x4000::SDT_YOURHASH::INSTR')
inst.timeout = 5000  # ms

# Identyfikacja urządzenia
print(inst.query('*IDN?'))
# Wynik: bartkepl,SDT,ABCDEF12,1.0

# Odczyt temperatury
temp = float(inst.query('SENSor:TEMPerature?'))
print(f'Temperatura: {temp:.4f} °C')

# Odczyt wilgotności (tylko SHT45)
hum = float(inst.query('SENSor:HUMidity?'))
print(f'Wilgotność: {hum:.2f} %RH')

# Typ czujnika
print(inst.query('SENSor:TYPE?'))
# Wynik: "SHT45", "TMP117" lub "DUAL"

inst.close()
```

### Przykład pomiaru ciągłego

```python
import pyvisa
import time

rm = pyvisa.ResourceManager('@py')
inst = rm.open_resource('USB0::0xCAFE::0x4000::SDT_YOURHASH::INSTR')
inst.timeout = 5000

# Ustaw okres odczytu czujnika na 500 ms
inst.write('SENSor:READperiod 500')

try:
    while True:
        temp = float(inst.query('SENSor:TEMPerature?'))
        print(f'{temp:.4f} °C')
        time.sleep(0.5)
except KeyboardInterrupt:
    pass

inst.close()
```

---

## Python — obsługa błędów SCPI

```python
# Sprawdź kolejkę błędów po każdej operacji
def check_errors(inst):
    count = int(inst.query('SYSTem:ERRor:COUNt?'))
    for _ in range(count):
        err = inst.query('SYSTem:ERRor:NEXT?')
        print(f'Błąd SCPI: {err}')

inst.write('SENSor:ALERt:HIGH 200')  # zły zakres
check_errors(inst)
# Błąd SCPI: -222,"Data out of range"
```

---

## Terminal SCPI (ręczny)

Możesz komunikować się z urządzeniem przez dowolny terminal VISA lub przez interaktywny REPL Python z PyVISA.

Przykładowe komendy do wpisania ręcznie:

```
*IDN?
SENSor:TYPE?
SENSor:TEMPerature?
DISPlay:BRIGhtness 50
DISPlay:TEXT "HELLO   "
DISPlay:SOURce 1
```

---

## Wyświetlacz — sterowanie

```python
# Wyłącz wyświetlacz
inst.write('DISPlay:STATe OFF')

# Ustaw jasność 30%
inst.write('DISPlay:BRIGhtness 30')

# Wyświetl własny tekst (max 8 znaków)
inst.write('DISPlay:SOURce 1')       # przełącz na tryb tekstowy
inst.write('DISPlay:TEXT "TEST    "')

# Wróć do wyświetlania pomiarów
inst.write('DISPlay:SOURce 0')
```

---

## Wejście w tryb bootloadera DFU

Aby zaktualizować firmware przez USB DFU:

```python
inst.write('SYSTem:BOOTloader:ENter')
```

!!! warning "Uwaga"
    Po wykonaniu tej komendy urządzenie natychmiast wejdzie w tryb DFU i przestanie odpowiadać przez SCPI. Do wgrania firmware użyj narzędzia **STM32CubeProgrammer** lub `dfu-util`.

---

## Reset urządzenia

```python
# Reset przez SCPI
inst.write('SYSTem:RST')

# Lub standardowy IEEE 488.2
inst.write('*RST')
```
