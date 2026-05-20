"""
SDT Board Companion — main.py
Complete SCPI GUI for the SDT Board (STM32C071, USB-TMC).
Requires: pyvisa, matplotlib; optional: ttkthemes
"""

from __future__ import annotations

import csv
import dataclasses
import math
import shutil
import subprocess
import threading
import time
import tkinter as tk
from datetime import datetime
from tkinter import filedialog, messagebox, scrolledtext, ttk
from typing import Callable, List, Optional

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import pyvisa

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

CONV_RATE_LABELS = {
    0: "15.5 ms", 1: "125 ms", 2: "250 ms", 3: "500 ms",
    4: "1 s",     5: "4 s",   6: "8 s",   7: "16 s",
}
TMP117_AVG_VALUES = (1, 8, 32, 64)
SCPI_NAN_THRESHOLD = 9e36
DFU_WAIT_SECONDS = 3
LOG_MAX_LINES = 2000

DFU_TOOL_CANDIDATES = [
    ("dfu-util",            ["dfu-util", "--version"]),
    ("STM32_Programmer_CLI", ["STM32_Programmer_CLI", "--version"]),
]

# ---------------------------------------------------------------------------
# Localisation (EN / PL)
# ---------------------------------------------------------------------------

_LANGS: dict[str, dict[str, str]] = {
    "EN": {
        # ── Common buttons ────────────────────────────────────────────────
        "btn_set":          "Set",
        "btn_get":          "Get",
        "btn_read":         "Read",
        "btn_query":        "Query",
        "btn_refresh":      "Refresh",
        "btn_connect":      "Connect",
        "btn_disconnect":   "Disconnect",
        "btn_browse":       "Browse…",
        "btn_clear_log":    "Clear Log",
        "btn_clear_data":   "Clear Data",
        "btn_clear_output": "Clear Output",
        "btn_send":         "Send",
        "btn_write":        "Write",
        # ── ConnectionTab ─────────────────────────────────────────────────
        "frame_visa":           "VISA Resource",
        "frame_dev_info":       "Device Info",
        "frame_tips":           "Quick Tips",
        "conn_tip_1": "• Connect to the device then explore tabs for all SCPI commands.",
        "conn_tip_2": "• Console tab: send any arbitrary SCPI command with ↑/↓ history.",
        "conn_tip_3": "• DFU Programmer tab: flash new firmware directly from the GUI.",
        "conn_tip_4": "• Measurements tab: start cyclic polling and live plot.",
        "reconnect_fail_status":  "Status: Disconnected — reconnect failed",
        "reconnect_fail_log":     "Auto-reconnect failed after max attempts",
        "reconnect_attempt":      "Status: Reconnecting... (attempt {n}/{max})",
        "reconnect_ok_log":       "Auto-reconnect successful: {res}",
        "reconnect_ok_status":    "Status: Connected (auto) — {res}",
        # ── SensorTab ─────────────────────────────────────────────────────
        "frame_readings":       "Readings",
        "frame_configuration":  "Configuration",
        "frame_actions":        "Actions",
        "lbl_temperature":      "Temperature:",
        "lbl_humidity":         "Humidity:",
        "lbl_sensor_type":      "Sensor Type:",
        "lbl_sensor_id":        "Sensor ID:",
        "lbl_read_period":      "Read Period (ms) [50–60000]:",
        "lbl_average":          "Average (SHT45: 1–255 / TMP117: 1,8,32,64):",
        "lbl_precision":        "Precision (SHT45 only):",
        "btn_heater":           "Heater ON (SHT45)",
        "btn_soft_reset":       "Soft Reset",
        # ── TMP117Tab ─────────────────────────────────────────────────────
        "frame_alert":          "Alert Thresholds",
        "frame_conversion":     "Conversion",
        "lbl_alert_high":       "Alert High [°C]:",
        "lbl_alert_low":        "Alert Low [°C]:",
        "lbl_mode":             "Mode:",
        "lbl_conv_rate":        "Conv Rate (0–7):",
        "btn_alert_status":     "Query Alert Status",
        # ── DisplayTab ────────────────────────────────────────────────────
        "frame_brightness":     "Brightness",
        "frame_disp_state":     "Display State",
        "frame_disp_source":    "Display Source",
        "frame_disp_text":      "Display Text (max 8 characters)",
        "lbl_bright_range":     "1–100:",
        "lbl_disp_text":        "Text:",
        "btn_set_state":        "Set State",
        "btn_get_state":        "Get State",
        "btn_set_source":       "Set Source",
        "btn_get_source":       "Get Source",
        "disp_src_meas":        "Measurement (0)",
        "disp_src_text":        "User Text (1)",
        # ── SystemTab ─────────────────────────────────────────────────────
        "frame_ieee_regs":      "IEEE 488.2 Status Registers",
        "frame_selftest":       "Self-Test & Error Queue",
        "frame_sys_info":       "Device Info",
        "frame_device_ctrl":    "Device Control",
        "frame_misc_ieee":      "Misc IEEE 488.2",
        "lbl_ese":              "ESE (Event Status Enable):",
        "lbl_sre":              "SRE (Service Request Enable):",
        "lbl_esr":              "ESR (Event Status Register):",
        "lbl_stb":              "STB (Status Byte):",
        "lbl_scpi_ver":         "SCPI Version:",
        "btn_err_count":        "Error Count",
        "btn_read_errors":      "Read All Errors",
        "btn_clear_errors":     "Clear (*CLS)",
        "btn_bootloader":       "Enter Bootloader  (SYSTem:BOOTloader:ENter)",
        "btn_restart":          "Restart Device  (SYSTem:RST)",
        "btn_opc_set":          "*OPC (set bit)",
        # ── MeasurementsTab ───────────────────────────────────────────────
        "frame_poll_cfg":       "Polling Configuration",
        "frame_controls":       "Controls",
        "frame_live":           "Live Readings",
        "frame_csv":            "CSV Export",
        "lbl_interval":         "Interval [s]:",
        "lbl_samples":          "Samples:",
        "lbl_csv_file":         "File:",
        "btn_start_poll":       "▶  Start Polling",
        "btn_stop_poll":        "■  Stop Polling",
        "btn_open_plot":        "Open Plot",
        "btn_start_csv":        "Start CSV",
        "btn_stop_csv":         "Stop CSV",
        # ── CalibrationTab ────────────────────────────────────────────────
        "frame_cal_points":     "Calibration points  (T_measured, T_reference)",
        "frame_cal_degree":     "Polynomial degree",
        "frame_cal_actions":    "Actions",
        "frame_cal_residuals":  "Residuals  (T_calibrated − T_reference)",
        "lbl_t_measured":       "T measured [°C]:",
        "lbl_t_reference":      "T reference [°C]:",
        "btn_add_point":        "Add point",
        "btn_remove_point":     "Remove selected",
        "btn_fit":              "Fit polynomial",
        "btn_send_cal":         "Send to device",
        "btn_read_cal":         "Read from device",
        "btn_reset_cal":        "Reset to identity (no correction)",
        # ── DFUTab ────────────────────────────────────────────────────────
        "frame_fw_file":        "Firmware File (.hex or .elf)",
        "frame_flash_tool":     "Flash Tool",
        "frame_flash_ctrl":     "Flash",
        "frame_flash_log":      "Flash Log",
        "btn_detect_tool":      "Detect Again",
        "btn_flash":            "Flash Firmware",
        # ── ConsoleTab ────────────────────────────────────────────────────
        "frame_scpi_cmd":       "SCPI Command",
        "frame_response":       "Response / Output",
        "lbl_command":          "Command:",
        "console_hint":         "↑/↓ for history  •  Commands ending with ? are queried; others are written",
        # ── ConfigTab ─────────────────────────────────────────────────────
        "cfg_title":            "Three-tier Configuration System",
        "cfg_status_frame":     "Configuration Status",
        "cfg_operations":       "Operations",
        "cfg_dirty_label":      "Unsaved RAM changes:",
        "cfg_refresh_btn":      "Refresh",
        "cfg_save_desc":        "Save current RAM config → PRIMARY + BACKUP (with CRC)",
        "cfg_restore_desc":     "Restore PRIMARY from BACKUP (repair corrupted PRIMARY)",
        "cfg_recall_desc":      "Restore factory defaults (DEFAULT → PRIMARY + BACKUP)",
        "cfg_yes":              "YES",
        "cfg_no":               "NO",
        "cfg_info_text": (
            "DEFAULT  — factory defaults, baked into FLASH as part of firmware (addr 0x0801C000).\n"
            "PRIMARY  — active config, loaded on startup (addr 0x0801C800).\n"
            "BACKUP   — copy of PRIMARY, used when PRIMARY is corrupted (addr 0x0801D000).\n\n"
            "Parameter changes (sensor, display) are held in RAM until explicitly saved\n"
            "with SAVE. On reset the device reloads PRIMARY from FLASH."
        ),
        "cfg_restore_msg":      "Restore PRIMARY from BACKUP?\nUnsaved changes in PRIMARY will be lost.",
        "cfg_recall_msg":       "Restore factory defaults (DEFAULT)?\nPRIMARY and BACKUP will be overwritten.",
    },

    "PL": {
        # ── Common buttons ────────────────────────────────────────────────
        "btn_set":          "Ustaw",
        "btn_get":          "Pobierz",
        "btn_read":         "Odczytaj",
        "btn_query":        "Zapytaj",
        "btn_refresh":      "Odśwież",
        "btn_connect":      "Połącz",
        "btn_disconnect":   "Rozłącz",
        "btn_browse":       "Przeglądaj…",
        "btn_clear_log":    "Wyczyść log",
        "btn_clear_data":   "Wyczyść dane",
        "btn_clear_output": "Wyczyść wyjście",
        "btn_send":         "Wyślij",
        "btn_write":        "Zapisz",
        # ── ConnectionTab ─────────────────────────────────────────────────
        "frame_visa":           "Zasób VISA",
        "frame_dev_info":       "Info o urządzeniu",
        "frame_tips":           "Wskazówki",
        "conn_tip_1": "• Połącz urządzenie, a następnie eksploruj zakładki dla komend SCPI.",
        "conn_tip_2": "• Zakładka Konsola: wyślij dowolną komendę SCPI z historią ↑/↓.",
        "conn_tip_3": "• Zakładka DFU: wgraj firmware bezpośrednio z GUI.",
        "conn_tip_4": "• Zakładka Pomiary: uruchom cykliczny polling i wykres live.",
        "reconnect_fail_status":  "Status: Rozłączono — reconnect nieudany",
        "reconnect_fail_log":     "Auto-reconnect nieudany po wielu próbach",
        "reconnect_attempt":      "Status: Reconnect... (próba {n}/{max})",
        "reconnect_ok_log":       "Auto-reconnect udany: {res}",
        "reconnect_ok_status":    "Status: Połączono (auto) — {res}",
        # ── SensorTab ─────────────────────────────────────────────────────
        "frame_readings":       "Odczyty",
        "frame_configuration":  "Konfiguracja",
        "frame_actions":        "Akcje",
        "lbl_temperature":      "Temperatura:",
        "lbl_humidity":         "Wilgotność:",
        "lbl_sensor_type":      "Typ czujnika:",
        "lbl_sensor_id":        "ID czujnika:",
        "lbl_read_period":      "Okres odczytu (ms) [50–60000]:",
        "lbl_average":          "Uśrednianie (SHT45: 1–255 / TMP117: 1,8,32,64):",
        "lbl_precision":        "Precyzja (tylko SHT45):",
        "btn_heater":           "Grzałka ON (SHT45)",
        "btn_soft_reset":       "Miękki reset",
        # ── TMP117Tab ─────────────────────────────────────────────────────
        "frame_alert":          "Progi alertów",
        "frame_conversion":     "Konwersja",
        "lbl_alert_high":       "Próg wysoki [°C]:",
        "lbl_alert_low":        "Próg niski [°C]:",
        "lbl_mode":             "Tryb:",
        "lbl_conv_rate":        "Częst. konwersji (0–7):",
        "btn_alert_status":     "Odczytaj status alertu",
        # ── DisplayTab ────────────────────────────────────────────────────
        "frame_brightness":     "Jasność",
        "frame_disp_state":     "Stan wyświetlacza",
        "frame_disp_source":    "Źródło wyświetlacza",
        "frame_disp_text":      "Tekst wyświetlacza (maks. 8 znaków)",
        "lbl_bright_range":     "1–100:",
        "lbl_disp_text":        "Tekst:",
        "btn_set_state":        "Ustaw stan",
        "btn_get_state":        "Pobierz stan",
        "btn_set_source":       "Ustaw źródło",
        "btn_get_source":       "Pobierz źródło",
        "disp_src_meas":        "Pomiar (0)",
        "disp_src_text":        "Tekst użytkownika (1)",
        # ── SystemTab ─────────────────────────────────────────────────────
        "frame_ieee_regs":      "Rejestry statusu IEEE 488.2",
        "frame_selftest":       "Autotest i kolejka błędów",
        "frame_sys_info":       "Info o urządzeniu",
        "frame_device_ctrl":    "Sterowanie urządzeniem",
        "frame_misc_ieee":      "Inne IEEE 488.2",
        "lbl_ese":              "ESE (Event Status Enable):",
        "lbl_sre":              "SRE (Service Request Enable):",
        "lbl_esr":              "ESR (Event Status Register):",
        "lbl_stb":              "STB (Status Byte):",
        "lbl_scpi_ver":         "Wersja SCPI:",
        "btn_err_count":        "Liczba błędów",
        "btn_read_errors":      "Odczytaj wszystkie błędy",
        "btn_clear_errors":     "Wyczyść (*CLS)",
        "btn_bootloader":       "Wejdź w bootloader  (SYSTem:BOOTloader:ENter)",
        "btn_restart":          "Restart urządzenia  (SYSTem:RST)",
        "btn_opc_set":          "*OPC (ustaw bit)",
        # ── MeasurementsTab ───────────────────────────────────────────────
        "frame_poll_cfg":       "Konfiguracja pollingu",
        "frame_controls":       "Sterowanie",
        "frame_live":           "Odczyty na żywo",
        "frame_csv":            "Eksport CSV",
        "lbl_interval":         "Interwał [s]:",
        "lbl_samples":          "Próbki:",
        "lbl_csv_file":         "Plik:",
        "btn_start_poll":       "▶  Rozpocznij polling",
        "btn_stop_poll":        "■  Zatrzymaj polling",
        "btn_open_plot":        "Otwórz wykres",
        "btn_start_csv":        "Start CSV",
        "btn_stop_csv":         "Stop CSV",
        # ── CalibrationTab ────────────────────────────────────────────────
        "frame_cal_points":     "Punkty kalibracyjne  (T_zmierzona, T_referencyjna)",
        "frame_cal_degree":     "Stopień wielomianu",
        "frame_cal_actions":    "Akcje",
        "frame_cal_residuals":  "Reszty  (T_skalibrowana − T_referencyjna)",
        "lbl_t_measured":       "T zmierzona [°C]:",
        "lbl_t_reference":      "T referencyjna [°C]:",
        "btn_add_point":        "Dodaj punkt",
        "btn_remove_point":     "Usuń zaznaczony",
        "btn_fit":              "Dopasuj wielomian",
        "btn_send_cal":         "Wyślij do urządzenia",
        "btn_read_cal":         "Odczytaj z urządzenia",
        "btn_reset_cal":        "Reset do identyczności (brak korekcji)",
        # ── DFUTab ────────────────────────────────────────────────────────
        "frame_fw_file":        "Plik firmware (.hex lub .elf)",
        "frame_flash_tool":     "Narzędzie flash",
        "frame_flash_ctrl":     "Programowanie",
        "frame_flash_log":      "Log programowania",
        "btn_detect_tool":      "Wykryj ponownie",
        "btn_flash":            "Wgraj firmware",
        # ── ConsoleTab ────────────────────────────────────────────────────
        "frame_scpi_cmd":       "Komenda SCPI",
        "frame_response":       "Odpowiedź / Wyjście",
        "lbl_command":          "Komenda:",
        "console_hint":         "↑/↓ historia  •  Komendy z ? to zapytania; pozostałe są wysyłane",
        # ── ConfigTab ─────────────────────────────────────────────────────
        "cfg_title":            "Trójwarstwowy system konfiguracji",
        "cfg_status_frame":     "Stan konfiguracji",
        "cfg_operations":       "Operacje",
        "cfg_dirty_label":      "Niezapisane zmiany w RAM:",
        "cfg_refresh_btn":      "Odśwież",
        "cfg_save_desc":        "Zapisz bieżącą konfigurację RAM → PRIMARY + BACKUP (z CRC)",
        "cfg_restore_desc":     "Przywróć PRIMARY z BACKUP (naprawa uszkodzonego PRIMARY)",
        "cfg_recall_desc":      "Przywróć ustawienia fabryczne (DEFAULT → PRIMARY + BACKUP)",
        "cfg_yes":              "TAK",
        "cfg_no":               "NIE",
        "cfg_info_text": (
            "DEFAULT  — wartości fabryczne, zapisane w FLASH jako część firmware (adres 0x0801C000).\n"
            "PRIMARY  — aktywna konfiguracja, wczytywana przy starcie (adres 0x0801C800).\n"
            "BACKUP   — kopia PRIMARY, używana gdy PRIMARY jest uszkodzony (adres 0x0801D000).\n\n"
            "Zmiany parametrów (czujnik, wyświetlacz) są przechowywane w RAM do momentu\n"
            "jawnego zapisu komendą SAVE. Po resecie urządzenie wczytuje PRIMARY z FLASH."
        ),
        "cfg_restore_msg":      "Przywrócić PRIMARY z bloku BACKUP?\nBieżące niezapisane zmiany w PRIMARY zostaną utracone.",
        "cfg_recall_msg":       "Przywrócić ustawienia fabryczne (DEFAULT)?\nPRIMARY i BACKUP zostaną nadpisane wartościami domyślnymi.",
    },
}

_current_lang: str = "EN"


def tr(key: str, **kwargs: object) -> str:
    """Return a translated string for the current language."""
    return _LANGS[_current_lang][key].format(**kwargs)


# ---------------------------------------------------------------------------
# SCPIDevice — thin pyvisa wrapper
# ---------------------------------------------------------------------------

class SCPIDevice:
    def __init__(self):
        self.rm = pyvisa.ResourceManager()
        self._inst = None

    def list_resources(self) -> tuple:
        return self.rm.list_resources()

    def connect(self, resource_name: str, timeout_ms: int = 5000) -> None:
        self._inst = self.rm.open_resource(resource_name)
        self._inst.timeout = timeout_ms

    def disconnect(self) -> None:
        if self._inst is not None:
            try:
                self._inst.close()
            except Exception:
                pass
            self._inst = None

    def is_connected(self) -> bool:
        return self._inst is not None

    def query(self, cmd: str) -> str:
        if not self.is_connected():
            raise ConnectionError("Not connected to device")
        return self._inst.query(cmd).strip()

    def write(self, cmd: str) -> None:
        if not self.is_connected():
            raise ConnectionError("Not connected to device")
        self._inst.write(cmd)

    @staticmethod
    def parse_float(text: str) -> Optional[float]:
        try:
            val = float(text)
            if math.isnan(val) or abs(val) > SCPI_NAN_THRESHOLD:
                return None
            return val
        except (ValueError, TypeError):
            return None

    @staticmethod
    def parse_int(text: str) -> Optional[int]:
        try:
            return int(text)
        except (ValueError, TypeError):
            return None


# ---------------------------------------------------------------------------
# AppState — shared mutable state (no widgets)
# ---------------------------------------------------------------------------

@dataclasses.dataclass
class AppState:
    sensor_type: Optional[str] = None
    polling: bool = False
    poll_after_id: Optional[str] = None
    timestamps: List[float] = dataclasses.field(default_factory=list)
    temp_values: List[float] = dataclasses.field(default_factory=list)
    hum_values: List[float] = dataclasses.field(default_factory=list)
    start_time: Optional[float] = None
    csv_file: object = None
    csv_writer: object = None
    dfu_thread: Optional[threading.Thread] = None

    def clear_measurements(self):
        self.timestamps.clear()
        self.temp_values.clear()
        self.hum_values.clear()
        self.start_time = None


# ---------------------------------------------------------------------------
# _ScpiMixin — shared SCPI helpers for all tab classes
# ---------------------------------------------------------------------------

class _ScpiMixin:
    # Subclasses must set: self.device, self.log_pane, self.status_bar

    def safe_query(self, cmd: str) -> Optional[str]:
        try:
            resp = self.device.query(cmd)
            self.log_pane.log(f"{cmd} → {resp}", "ok")
            self.status_bar.set_last_cmd(cmd)
            return resp
        except (pyvisa.errors.VisaIOError, ConnectionError):
            self.log_pane.log(f"COMM ERROR [{cmd}]: connection lost", "error")
            self.status_bar.set_last_cmd(cmd)
            self.device.disconnect()
            if self.on_comm_error:
                self.on_comm_error()
            return None
        except Exception as e:
            self.log_pane.log(f"ERROR [{cmd}]: {e}", "error")
            self.status_bar.set_last_cmd(cmd)
            return None

    def safe_write(self, cmd: str) -> bool:
        try:
            self.device.write(cmd)
            self.log_pane.log(f"{cmd}", "ok")
            self.status_bar.set_last_cmd(cmd)
            return True
        except (pyvisa.errors.VisaIOError, ConnectionError):
            self.log_pane.log(f"COMM ERROR [{cmd}]: connection lost", "error")
            self.status_bar.set_last_cmd(cmd)
            self.device.disconnect()
            if self.on_comm_error:
                self.on_comm_error()
            return False
        except Exception as e:
            self.log_pane.log(f"ERROR [{cmd}]: {e}", "error")
            self.status_bar.set_last_cmd(cmd)
            messagebox.showerror("SCPI Error", str(e))
            return False

    def require_connection(self) -> bool:
        if not self.device.is_connected():
            self.log_pane.log("Not connected to device", "warn")
            messagebox.showerror("Not Connected", "Please connect to a device first.")
            return False
        return True


# ---------------------------------------------------------------------------
# StatusBar
# ---------------------------------------------------------------------------

class StatusBar:
    def __init__(self, parent: tk.Widget):
        self.frame = ttk.Frame(parent, relief="sunken", padding=(4, 2))

        try:
            frame_bg = ttk.Style().lookup("TFrame", "background") or "SystemButtonFace"
        except Exception:
            frame_bg = "SystemButtonFace"
        self._dot_canvas = tk.Canvas(self.frame, width=14, height=14,
                                     highlightthickness=0, bg=frame_bg)
        self._dot_canvas.pack(side=tk.LEFT, padx=(2, 0))
        self._dot = self._dot_canvas.create_oval(2, 2, 12, 12, fill="#cc3333", outline="")

        self._conn_lbl = ttk.Label(self.frame, text="Disconnected", anchor="w", width=38)
        self._conn_lbl.pack(side=tk.LEFT, padx=(2, 6))

        ttk.Separator(self.frame, orient="vertical").pack(side=tk.LEFT, fill=tk.Y, pady=1)

        self._cmd_lbl = ttk.Label(self.frame, text="", anchor="w", width=28,
                                  font=("Courier New", 9))
        self._cmd_lbl.pack(side=tk.LEFT, padx=6)

        ttk.Separator(self.frame, orient="vertical").pack(side=tk.LEFT, fill=tk.Y, pady=1)

        self._sensor_lbl = ttk.Label(self.frame, text="Sensor: —", anchor="w", width=18)
        self._sensor_lbl.pack(side=tk.LEFT, padx=6)

    def set_connected(self, resource_name: Optional[str]) -> None:
        if resource_name:
            self._dot_canvas.itemconfig(self._dot, fill="#33bb55")
            short = resource_name if len(resource_name) <= 38 else resource_name[:35] + "…"
            self._conn_lbl.config(text=f"Connected: {short}")
        else:
            self._dot_canvas.itemconfig(self._dot, fill="#cc3333")
            self._conn_lbl.config(text="Disconnected")

    def set_last_cmd(self, cmd: str) -> None:
        short = cmd if len(cmd) <= 28 else cmd[:25] + "…"
        self._cmd_lbl.config(text=short)

    def set_sensor_type(self, stype: Optional[str]) -> None:
        self._sensor_lbl.config(text=f"Sensor: {stype or '—'}")


# ---------------------------------------------------------------------------
# LogPane
# ---------------------------------------------------------------------------

class LogPane:
    def __init__(self, parent: tk.Widget, height: int = 7):
        self.frame = ttk.LabelFrame(parent, text="Log")
        inner = ttk.Frame(self.frame)
        inner.pack(fill=tk.BOTH, expand=True, padx=4, pady=2)

        self._text = tk.Text(inner, height=height, wrap=tk.WORD,
                             font=("Courier New", 9), state="disabled")
        self._text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        sb = ttk.Scrollbar(inner, orient="vertical", command=self._text.yview)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        self._text.config(yscrollcommand=sb.set)

        self._text.tag_configure("ok",    foreground="#22aa44")
        self._text.tag_configure("error", foreground="#cc2222")
        self._text.tag_configure("warn",  foreground="#cc8800")
        self._text.tag_configure("info",  foreground="")

        btn_row = ttk.Frame(self.frame)
        btn_row.pack(fill=tk.X, padx=4, pady=(0, 2))
        ttk.Button(btn_row, text="Clear Log", command=self.clear).pack(side=tk.RIGHT)

    def log(self, text: str, tag: str = "info") -> None:
        ts = time.strftime("%H:%M:%S")
        self._text.config(state="normal")
        self._text.insert(tk.END, f"[{ts}] {text}\n", tag)
        # Trim excess lines
        lines = int(self._text.index("end-1c").split(".")[0])
        if lines > LOG_MAX_LINES:
            self._text.delete("1.0", f"{lines - LOG_MAX_LINES}.0")
        self._text.see(tk.END)
        self._text.config(state="disabled")

    def clear(self) -> None:
        self._text.config(state="normal")
        self._text.delete("1.0", tk.END)
        self._text.config(state="disabled")


# ---------------------------------------------------------------------------
# PlotWindow
# ---------------------------------------------------------------------------

class PlotWindow:
    def __init__(self, parent_root: tk.Tk, state: AppState):
        self._root = parent_root
        self._state = state
        self._win: Optional[tk.Toplevel] = None
        self._fig = self._ax_temp = self._ax_hum = None
        self._line_temp = self._line_hum = None
        self._canvas = None
        self._series_var: Optional[tk.StringVar] = None

    @property
    def is_open(self) -> bool:
        return self._win is not None

    def open(self):
        if self.is_open:
            self._win.lift()
            return
        self._win = tk.Toplevel(self._root)
        self._win.title("Live Measurement Plot")
        self._win.protocol("WM_DELETE_WINDOW", self.close)

        toolbar = ttk.Frame(self._win)
        toolbar.pack(fill=tk.X, padx=6, pady=4)

        ttk.Label(toolbar, text="Show:").pack(side=tk.LEFT)
        self._series_var = tk.StringVar(value="TEMP")
        for text, val in (("Temperature", "TEMP"), ("Humidity", "HUM"), ("Both", "BOTH")):
            ttk.Radiobutton(toolbar, text=text, variable=self._series_var,
                            value=val, command=self._update_visibility
                            ).pack(side=tk.LEFT, padx=4)
        ttk.Button(toolbar, text="Clear", command=self._clear).pack(side=tk.RIGHT)

        self._fig, (self._ax_temp, self._ax_hum) = plt.subplots(2, 1, figsize=(9, 6),
                                                                  tight_layout=True)
        self._line_temp, = self._ax_temp.plot([], [], "-o", linewidth=1.5, markersize=3,
                                               color="#e05533", label="Temperature")
        self._line_hum,  = self._ax_hum.plot([], [], "-s", linewidth=1.5, markersize=3,
                                               color="#3366cc", label="Humidity")
        self._ax_temp.set_title("Temperature", fontsize=11, fontweight="bold")
        self._ax_temp.set_xlabel("Time [s]")
        self._ax_temp.set_ylabel("°C")
        self._ax_temp.grid(True, alpha=0.3)
        self._ax_hum.set_title("Humidity", fontsize=11, fontweight="bold")
        self._ax_hum.set_xlabel("Time [s]")
        self._ax_hum.set_ylabel("%RH")
        self._ax_hum.grid(True, alpha=0.3)

        self._canvas = FigureCanvasTkAgg(self._fig, master=self._win)
        self._canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        self._update_visibility()

    def update(self):
        if not self.is_open:
            return
        s = self._state
        self._line_temp.set_data(s.timestamps, s.temp_values)
        self._line_hum.set_data(
            s.timestamps[:len(s.hum_values)], s.hum_values)
        for ax in (self._ax_temp, self._ax_hum):
            ax.relim()
            ax.autoscale_view()
        self._canvas.draw_idle()

    def _update_visibility(self):
        if not self.is_open:
            return
        series = self._series_var.get()
        show_temp = series in ("TEMP", "BOTH")
        show_hum  = series in ("HUM",  "BOTH")
        self._ax_temp.set_visible(show_temp)
        self._ax_hum.set_visible(show_hum)
        if show_temp and show_hum:
            self._fig.set_size_inches(9, 6)
        else:
            self._fig.set_size_inches(9, 3.5)
        self._canvas.draw_idle()

    def _clear(self):
        self._state.clear_measurements()
        self.update()

    def close(self):
        if self._win:
            self._win.destroy()
        self._win = self._fig = self._ax_temp = self._ax_hum = None
        self._line_temp = self._line_hum = self._canvas = None


# ===========================================================================
# Tab base class
# ===========================================================================

class _BaseTab(_ScpiMixin):
    def __init__(self, notebook: ttk.Notebook, state: AppState,
                 device: SCPIDevice, log_pane: LogPane, status_bar: StatusBar):
        self.notebook = notebook
        self.state = state
        self.device = device
        self.log_pane = log_pane
        self.status_bar = status_bar
        self.frame = ttk.Frame(notebook, padding=6)
        self._connected_widgets: List[tk.Widget] = []
        self.on_comm_error: Optional[Callable] = None
        self._tr_items: list = []  # (widget, key, format_kwargs)

    def _tr(self, widget, key: str, **kwargs) -> object:
        """Register a Label/Button for automatic text refresh on language change."""
        self._tr_items.append((widget, key, kwargs))
        return widget

    def _tr_frame(self, frame: ttk.LabelFrame, key: str) -> ttk.LabelFrame:
        """Register a LabelFrame title using a StringVar+labelwidget.
        ttk.LabelFrame.config(text=...) does not reliably refresh on all platforms;
        labelwidget with textvariable is the guaranteed approach."""
        var = tk.StringVar(value=tr(key))
        lbl = ttk.Label(frame, textvariable=var)
        frame.configure(labelwidget=lbl)
        # store StringVar (not widget) — refresh_lang distinguishes by type
        self._tr_items.append((var, key, {}))
        return frame

    def refresh_lang(self) -> None:
        """Update all registered translatable widgets to the current language."""
        for item, key, kwargs in self._tr_items:
            if isinstance(item, tk.StringVar):
                item.set(tr(key, **kwargs))
            else:
                item.config(text=tr(key, **kwargs))

    def _sync_ui_state(self, connected: bool):
        state = "normal" if connected else "disabled"
        for w in self._connected_widgets:
            try:
                w.config(state=state)
            except tk.TclError:
                pass

    def _trigger_comm_error(self):
        """Disconnect device and notify the app (safe from any thread)."""
        self.device.disconnect()
        if self.on_comm_error:
            self.frame.after(0, self.on_comm_error)


# ===========================================================================
# ConnectionTab
# ===========================================================================

class ConnectionTab(_BaseTab):
    _RECONNECT_INTERVAL_MS = 3000
    _RECONNECT_MAX_ATTEMPTS = 6   # 6 × 3s = 18s

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.on_connect_callback: Optional[Callable] = None
        self.on_disconnect_callback: Optional[Callable] = None
        self.on_lang_change: Optional[Callable] = None
        self._last_resource: Optional[str] = None
        self._reconnect_attempt: int = 0
        self._reconnect_pending: bool = False
        self._build()

    def _build(self):
        # --- VISA Resource ---
        res_frame = ttk.LabelFrame(self.frame, text=tr("frame_visa"))
        self._tr_frame(res_frame, "frame_visa")
        res_frame.pack(fill=tk.X, pady=(0, 6))

        self._resource_combo = ttk.Combobox(res_frame, width=46)
        self._resource_combo.grid(row=0, column=0, padx=5, pady=5)
        b_ref = ttk.Button(res_frame, text=tr("btn_refresh"),    command=self._on_refresh)
        b_con = ttk.Button(res_frame, text=tr("btn_connect"),    command=self._on_connect)
        b_dis = ttk.Button(res_frame, text=tr("btn_disconnect"), command=self._on_disconnect)
        self._tr(b_ref, "btn_refresh")
        self._tr(b_con, "btn_connect")
        self._tr(b_dis, "btn_disconnect")
        b_ref.grid(row=0, column=1, padx=2)
        b_con.grid(row=0, column=2, padx=2)
        b_dis.grid(row=0, column=3, padx=2)

        self._conn_status_lbl = ttk.Label(res_frame, text="Status: Disconnected",
                                          foreground="#cc3333")
        self._conn_status_lbl.grid(row=1, column=0, columnspan=4, sticky="w", padx=5, pady=(0, 4))

        # --- Device Info ---
        info_frame = ttk.LabelFrame(self.frame, text=tr("frame_dev_info"))
        self._tr_frame(info_frame, "frame_dev_info")
        info_frame.pack(fill=tk.X, pady=(0, 6))

        idn_btn = ttk.Button(info_frame, text="*IDN?", command=self._on_idn, width=10)
        idn_btn.grid(row=0, column=0, padx=6, pady=5)
        self._idn_lbl = ttk.Label(info_frame, text="—", anchor="w", font=("Courier New", 9))
        self._idn_lbl.grid(row=0, column=1, sticky="ew", padx=4)
        info_frame.columnconfigure(1, weight=1)
        self._connected_widgets.append(idn_btn)

        ver_btn = ttk.Button(info_frame, text="SYST:VER?", command=self._on_sys_ver, width=10)
        ver_btn.grid(row=1, column=0, padx=6, pady=(0, 5))
        self._ver_lbl = ttk.Label(info_frame, text="—", anchor="w")
        self._ver_lbl.grid(row=1, column=1, sticky="ew", padx=4)
        self._connected_widgets.append(ver_btn)

        # --- Quick Tips ---
        tip_frame = ttk.LabelFrame(self.frame, text=tr("frame_tips"))
        self._tr_frame(tip_frame, "frame_tips")
        tip_frame.pack(fill=tk.X, pady=(0, 6))
        tip_keys = ("conn_tip_1", "conn_tip_2", "conn_tip_3", "conn_tip_4")
        self._tip_labels = []
        for i, key in enumerate(tip_keys):
            lbl = ttk.Label(tip_frame, text=tr(key), anchor="w")
            self._tr(lbl, key)
            lbl.grid(row=i, column=0, sticky="w", padx=8, pady=1)
            self._tip_labels.append(lbl)

        # --- Language ---
        lang_frame = ttk.LabelFrame(self.frame, text="Language / Język")
        lang_frame.pack(fill=tk.X, pady=(0, 6))
        self._lang_var = tk.StringVar(value=_current_lang)
        for text, code in (("English", "EN"), ("Polski", "PL")):
            ttk.Radiobutton(lang_frame, text=text, variable=self._lang_var,
                            value=code, command=self._on_lang_change
                            ).pack(side=tk.LEFT, padx=10, pady=4)

        self._sync_ui_state(False)
        self._on_refresh()

    def _on_lang_change(self):
        global _current_lang
        _current_lang = self._lang_var.get()
        if self.on_lang_change:
            self.on_lang_change()

    def _on_refresh(self):
        try:
            resources = self.device.list_resources()
            self._resource_combo["values"] = resources
            if resources:
                self._resource_combo.current(0)
            self.log_pane.log(f"Found {len(resources)} VISA resource(s)", "info")
        except Exception as e:
            self.log_pane.log(f"Failed to list resources: {e}", "error")

    def _on_connect(self):
        resource = self._resource_combo.get()
        if not resource:
            messagebox.showerror("Error", "Select a VISA resource first")
            return
        try:
            self.device.connect(resource)
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))
            self.log_pane.log(f"Connection failed: {e}", "error")
            return

        self._last_resource = resource
        self._reconnect_pending = False
        self.log_pane.log(f"Connected to {resource}", "ok")
        self._conn_status_lbl.config(text=f"Status: Connected — {resource}",
                                     foreground="#22aa44")
        self._sync_ui_state(True)

        sensor_type = None
        resp = self.safe_query("SENSor:TYPE?")
        if resp:
            sensor_type = resp.strip('"')
            self.state.sensor_type = sensor_type

        if self.on_connect_callback:
            self.on_connect_callback(resource, sensor_type)

    def _on_disconnect(self):
        if not self.device.is_connected():
            messagebox.showerror("Error", "Not connected")
            return
        self._last_resource = None   # manual disconnect — no auto-reconnect
        self._reconnect_pending = False
        self.device.disconnect()
        self.state.sensor_type = None
        self._conn_status_lbl.config(text="Status: Disconnected", foreground="#cc3333")
        self._sync_ui_state(False)
        self.log_pane.log("Disconnected", "warn")
        if self.on_disconnect_callback:
            self.on_disconnect_callback()

    def _handle_external_disconnect(self):
        """Called when a comm error caused an unexpected disconnect."""
        self._reconnect_pending = False
        self.state.sensor_type = None
        self._sync_ui_state(False)
        if self.on_disconnect_callback:
            self.on_disconnect_callback()
        if self._last_resource:
            self._reconnect_attempt = 0
            self._schedule_reconnect()

    def _schedule_reconnect(self):
        if self._reconnect_pending:
            return
        if self._reconnect_attempt >= self._RECONNECT_MAX_ATTEMPTS:
            self._conn_status_lbl.config(
                text=tr("reconnect_fail_status"), foreground="#cc3333")
            self.log_pane.log(tr("reconnect_fail_log"), "warn")
            return
        self._reconnect_pending = True
        self._reconnect_attempt += 1
        self._conn_status_lbl.config(
            text=tr("reconnect_attempt",
                    n=self._reconnect_attempt, max=self._RECONNECT_MAX_ATTEMPTS),
            foreground="#cc8800")
        self.frame.after(self._RECONNECT_INTERVAL_MS, self._try_reconnect)

    def _try_reconnect(self):
        self._reconnect_pending = False
        if self.device.is_connected():
            return  # already reconnected by user
        try:
            self.device.connect(self._last_resource)
        except Exception:
            self._schedule_reconnect()
            return
        # Success
        resource = self._last_resource
        self.log_pane.log(tr("reconnect_ok_log", res=resource), "ok")
        self._conn_status_lbl.config(
            text=tr("reconnect_ok_status", res=resource), foreground="#22aa44")
        self._sync_ui_state(True)
        sensor_type = None
        resp = self.safe_query("SENSor:TYPE?")
        if resp:
            sensor_type = resp.strip('"')
            self.state.sensor_type = sensor_type
        if self.on_connect_callback:
            self.on_connect_callback(resource, sensor_type)

    def _on_idn(self):
        resp = self.safe_query("*IDN?")
        if resp:
            self._idn_lbl.config(text=resp)

    def _on_sys_ver(self):
        resp = self.safe_query("SYSTem:VERSion?")
        if resp:
            self._ver_lbl.config(text=f"SCPI version: {resp}")

    def update_connected_state(self, connected: bool):
        self._sync_ui_state(connected)


# ===========================================================================
# SensorTab
# ===========================================================================

class SensorTab(_BaseTab):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._build()

    def _build(self):
        # --- Readings ---
        rd_frame = ttk.LabelFrame(self.frame, text=tr("frame_readings"))
        self._tr_frame(rd_frame, "frame_readings")
        rd_frame.pack(fill=tk.X, pady=(0, 6))
        rd_frame.columnconfigure(1, weight=1)

        rows = [
            ("lbl_temperature", "temp_val", "°C",  self._read_temperature),
            ("lbl_humidity",    "hum_val",  "%RH", self._read_humidity),
            ("lbl_sensor_type", "type_val", "",    self._query_sensor_type),
            ("lbl_sensor_id",   "id_val",   "",    self._query_sensor_id),
        ]
        self._rd_labels = {}
        for i, (lbl_key, val_key, unit, cmd) in enumerate(rows):
            lbl = ttk.Label(rd_frame, text=tr(lbl_key), anchor="w", width=15)
            self._tr(lbl, lbl_key)
            lbl.grid(row=i, column=0, padx=6, pady=3, sticky="w")
            val_lbl = ttk.Label(rd_frame, text="—", anchor="w", width=20,
                                font=("Courier New", 10, "bold"))
            val_lbl.grid(row=i, column=1, padx=4, sticky="ew")
            self._rd_labels[val_key] = val_lbl
            if unit:
                ttk.Label(rd_frame, text=unit).grid(row=i, column=2, padx=2)
            btn_key = "btn_read" if val_key in ("temp_val", "hum_val") else "btn_query"
            btn = ttk.Button(rd_frame, text=tr(btn_key), command=cmd, width=8)
            self._tr(btn, btn_key)
            btn.grid(row=i, column=3, padx=6)
            self._connected_widgets.append(btn)

        # --- Configuration ---
        cfg_frame = ttk.LabelFrame(self.frame, text=tr("frame_configuration"))
        self._tr_frame(cfg_frame, "frame_configuration")
        cfg_frame.pack(fill=tk.X, pady=(0, 6))

        # Read Period
        lbl_per = ttk.Label(cfg_frame, text=tr("lbl_read_period"))
        self._tr(lbl_per, "lbl_read_period")
        lbl_per.grid(row=0, column=0, sticky="w", padx=6, pady=3)
        self._period_entry = ttk.Entry(cfg_frame, width=10)
        self._period_entry.insert(0, "500")
        self._period_entry.grid(row=0, column=1, padx=4)
        set_per = ttk.Button(cfg_frame, text=tr("btn_set"), command=self._set_read_period, width=6)
        get_per = ttk.Button(cfg_frame, text=tr("btn_get"), command=self._get_read_period, width=6)
        self._tr(set_per, "btn_set"); self._tr(get_per, "btn_get")
        set_per.grid(row=0, column=2, padx=2)
        get_per.grid(row=0, column=3, padx=2)
        self._connected_widgets += [set_per, get_per]

        # Average
        lbl_avg = ttk.Label(cfg_frame, text=tr("lbl_average"))
        self._tr(lbl_avg, "lbl_average")
        lbl_avg.grid(row=1, column=0, sticky="w", padx=6, pady=3)
        self._avg_entry = ttk.Entry(cfg_frame, width=10)
        self._avg_entry.insert(0, "1")
        self._avg_entry.grid(row=1, column=1, padx=4)
        set_avg = ttk.Button(cfg_frame, text=tr("btn_set"), command=self._set_average, width=6)
        get_avg = ttk.Button(cfg_frame, text=tr("btn_get"), command=self._get_average, width=6)
        self._tr(set_avg, "btn_set"); self._tr(get_avg, "btn_get")
        set_avg.grid(row=1, column=2, padx=2)
        get_avg.grid(row=1, column=3, padx=2)
        self._connected_widgets += [set_avg, get_avg]

        # Precision (SHT45 only)
        lbl_prec = ttk.Label(cfg_frame, text=tr("lbl_precision"))
        self._tr(lbl_prec, "lbl_precision")
        lbl_prec.grid(row=2, column=0, sticky="w", padx=6, pady=3)
        self._precision_var = tk.StringVar(value="HIGH")
        self._precision_combo = ttk.Combobox(cfg_frame, textvariable=self._precision_var,
                                              values=["LOW", "MEDIUM", "HIGH"],
                                              state="readonly", width=10)
        self._precision_combo.grid(row=2, column=1, padx=4)
        set_prec = ttk.Button(cfg_frame, text=tr("btn_set"), command=self._set_precision, width=6)
        get_prec = ttk.Button(cfg_frame, text=tr("btn_get"), command=self._get_precision, width=6)
        self._tr(set_prec, "btn_set"); self._tr(get_prec, "btn_get")
        set_prec.grid(row=2, column=2, padx=2)
        get_prec.grid(row=2, column=3, padx=2)
        self._connected_widgets += [set_prec, get_prec]

        # --- Actions ---
        act_frame = ttk.LabelFrame(self.frame, text=tr("frame_actions"))
        self._tr_frame(act_frame, "frame_actions")
        act_frame.pack(fill=tk.X, pady=(0, 6))
        heater_btn = ttk.Button(act_frame, text=tr("btn_heater"), command=self._run_heater, width=22)
        self._tr(heater_btn, "btn_heater")
        heater_btn.grid(row=0, column=0, padx=8, pady=5)
        reset_btn = ttk.Button(act_frame, text=tr("btn_soft_reset"), command=self._soft_reset, width=14)
        self._tr(reset_btn, "btn_soft_reset")
        reset_btn.grid(row=0, column=1, padx=8)
        self._connected_widgets += [heater_btn, reset_btn]

        self._sync_ui_state(False)

    # Readings
    def _read_temperature(self):
        resp = self.safe_query("SENSor:TEMPerature?")
        if resp is not None:
            val = SCPIDevice.parse_float(resp)
            self._rd_labels["temp_val"].config(
                text=f"{val:.4f}" if val is not None else "N/A")

    def _read_humidity(self):
        resp = self.safe_query("SENSor:HUMidity?")
        if resp is not None:
            val = SCPIDevice.parse_float(resp)
            self._rd_labels["hum_val"].config(
                text=f"{val:.2f}" if val is not None else "N/A (TMP117)")

    def _query_sensor_type(self):
        resp = self.safe_query("SENSor:TYPE?")
        if resp is not None:
            cleaned = resp.strip('"')
            self._rd_labels["type_val"].config(text=cleaned)
            self.state.sensor_type = cleaned
            self.status_bar.set_sensor_type(cleaned)

    def _query_sensor_id(self):
        resp = self.safe_query("SENSor:ID?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            self._rd_labels["id_val"].config(
                text=f"0x{val:08X}" if val is not None else resp)

    # Configuration
    def _set_read_period(self):
        try:
            val = int(self._period_entry.get())
            if not (50 <= val <= 60000):
                raise ValueError("Period must be 50–60000 ms")
            self.safe_write(f"SENSor:READperiod {val}")
        except ValueError as e:
            messagebox.showerror("Validation Error", str(e))

    def _get_read_period(self):
        resp = self.safe_query("SENSor:READperiod?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            if val is not None:
                self._period_entry.delete(0, tk.END)
                self._period_entry.insert(0, str(val))

    def _set_average(self):
        try:
            val = int(self._avg_entry.get())
            if not (1 <= val <= 255):
                raise ValueError("Average must be 1–255")
            if self.state.sensor_type == "TMP117" and val not in TMP117_AVG_VALUES:
                if not messagebox.askyesno("Warning",
                        f"TMP117 only accepts 1, 8, 32, 64. Send {val} anyway?"):
                    return
            self.safe_write(f"SENSor:AVErage {val}")
        except ValueError as e:
            messagebox.showerror("Validation Error", str(e))

    def _get_average(self):
        resp = self.safe_query("SENSor:AVErage?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            if val is not None:
                self._avg_entry.delete(0, tk.END)
                self._avg_entry.insert(0, str(val))

    def _set_precision(self):
        self.safe_write(f"SENSor:PRECision {self._precision_var.get()}")

    def _get_precision(self):
        resp = self.safe_query("SENSor:PRECision?")
        if resp is not None:
            cleaned = resp.strip('"')
            val = SCPIDevice.parse_float(cleaned)
            if val is None and cleaned not in ("nan", "9.91E+37"):
                self._precision_var.set(cleaned)
            else:
                self.log_pane.log("  → Precision: N/A (TMP117)", "warn")

    def _run_heater(self):
        self.safe_write("SENSor:HEATer")

    def _soft_reset(self):
        self.safe_write("SENSor:SOFTReset")

    def update_sensor_availability(self, sensor_type: Optional[str]):
        pass  # Future: could dim heater for TMP117 etc.


# ===========================================================================
# TMP117Tab
# ===========================================================================

class TMP117Tab(_BaseTab):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._build()

    def _build(self):
        self._na_label = ttk.Label(
            self.frame,
            text="These settings apply to TMP117 only. Commands will return an error for SHT45.",
            foreground="#cc8800",
        )
        self._na_label.pack(anchor="w", pady=(0, 4))

        # --- Alert Thresholds ---
        alert_frame = ttk.LabelFrame(self.frame, text=tr("frame_alert"))
        self._tr_frame(alert_frame, "frame_alert")
        alert_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_ah = ttk.Label(alert_frame, text=tr("lbl_alert_high"))
        self._tr(lbl_ah, "lbl_alert_high")
        lbl_ah.grid(row=0, column=0, sticky="w", padx=6, pady=4)
        self._alert_high_entry = ttk.Entry(alert_frame, width=12)
        self._alert_high_entry.insert(0, "85.0")
        self._alert_high_entry.grid(row=0, column=1, padx=4)
        self._alert_high_cur = ttk.Label(alert_frame, text="", width=12, font=("Courier New", 9))
        self._alert_high_cur.grid(row=0, column=2, padx=4)
        set_ah = ttk.Button(alert_frame, text=tr("btn_set"), command=self._set_alert_high, width=6)
        get_ah = ttk.Button(alert_frame, text=tr("btn_get"), command=self._get_alert_high, width=6)
        self._tr(set_ah, "btn_set"); self._tr(get_ah, "btn_get")
        set_ah.grid(row=0, column=3, padx=2)
        get_ah.grid(row=0, column=4, padx=2)
        self._connected_widgets += [set_ah, get_ah]

        lbl_al = ttk.Label(alert_frame, text=tr("lbl_alert_low"))
        self._tr(lbl_al, "lbl_alert_low")
        lbl_al.grid(row=1, column=0, sticky="w", padx=6, pady=4)
        self._alert_low_entry = ttk.Entry(alert_frame, width=12)
        self._alert_low_entry.insert(0, "0.0")
        self._alert_low_entry.grid(row=1, column=1, padx=4)
        self._alert_low_cur = ttk.Label(alert_frame, text="", width=12, font=("Courier New", 9))
        self._alert_low_cur.grid(row=1, column=2, padx=4)
        set_al = ttk.Button(alert_frame, text=tr("btn_set"), command=self._set_alert_low, width=6)
        get_al = ttk.Button(alert_frame, text=tr("btn_get"), command=self._get_alert_low, width=6)
        self._tr(set_al, "btn_set"); self._tr(get_al, "btn_get")
        set_al.grid(row=1, column=3, padx=2)
        get_al.grid(row=1, column=4, padx=2)
        self._connected_widgets += [set_al, get_al]

        status_btn = ttk.Button(alert_frame, text=tr("btn_alert_status"), command=self._get_alert_status)
        self._tr(status_btn, "btn_alert_status")
        status_btn.grid(row=2, column=0, columnspan=2, padx=6, pady=4, sticky="w")
        self._alert_status_lbl = ttk.Label(alert_frame, text="—", font=("Courier New", 9, "bold"))
        self._alert_status_lbl.grid(row=2, column=2, columnspan=3, padx=4, sticky="w")
        self._connected_widgets.append(status_btn)

        # --- Conversion ---
        conv_frame = ttk.LabelFrame(self.frame, text=tr("frame_conversion"))
        self._tr_frame(conv_frame, "frame_conversion")
        conv_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_mode = ttk.Label(conv_frame, text=tr("lbl_mode"))
        self._tr(lbl_mode, "lbl_mode")
        lbl_mode.grid(row=0, column=0, sticky="w", padx=6, pady=4)
        self._mode_var = tk.StringVar(value="CONTINUOUS")
        self._mode_combo = ttk.Combobox(conv_frame, textvariable=self._mode_var,
                                         values=["CONTINUOUS", "SHUTDOWN", "ONESHOT"],
                                         state="readonly", width=14)
        self._mode_combo.grid(row=0, column=1, padx=4)
        set_mode = ttk.Button(conv_frame, text=tr("btn_set"), command=self._set_mode, width=6)
        get_mode = ttk.Button(conv_frame, text=tr("btn_get"), command=self._get_mode, width=6)
        self._tr(set_mode, "btn_set"); self._tr(get_mode, "btn_get")
        set_mode.grid(row=0, column=2, padx=2)
        get_mode.grid(row=0, column=3, padx=2)
        self._connected_widgets += [set_mode, get_mode]

        lbl_cr = ttk.Label(conv_frame, text=tr("lbl_conv_rate"))
        self._tr(lbl_cr, "lbl_conv_rate")
        lbl_cr.grid(row=1, column=0, sticky="w", padx=6, pady=4)
        self._conv_rate_var = tk.IntVar(value=4)
        self._conv_spin = ttk.Spinbox(conv_frame, from_=0, to=7,
                                       textvariable=self._conv_rate_var, width=5)
        self._conv_spin.grid(row=1, column=1, padx=4, sticky="w")
        self._conv_rate_lbl = ttk.Label(conv_frame, text=f"→ {CONV_RATE_LABELS[4]}",
                                         foreground="#555555", width=12)
        self._conv_rate_lbl.grid(row=1, column=2, padx=4, sticky="w")
        set_cr = ttk.Button(conv_frame, text=tr("btn_set"), command=self._set_conv_rate, width=6)
        get_cr = ttk.Button(conv_frame, text=tr("btn_get"), command=self._get_conv_rate, width=6)
        self._tr(set_cr, "btn_set"); self._tr(get_cr, "btn_get")
        set_cr.grid(row=1, column=3, padx=2)
        get_cr.grid(row=1, column=4, padx=2)
        self._connected_widgets += [set_cr, get_cr]
        self._conv_rate_var.trace_add("write", self._update_conv_rate_label)

        self._sync_ui_state(False)

    def _update_conv_rate_label(self, *_):
        try:
            rate = int(self._conv_rate_var.get())
            self._conv_rate_lbl.config(text=f"→ {CONV_RATE_LABELS.get(rate, '?')}")
        except (tk.TclError, ValueError):
            self._conv_rate_lbl.config(text="→ ?")

    def _set_alert_high(self):
        try:
            val = float(self._alert_high_entry.get())
            if not (-55.0 <= val <= 150.0):
                raise ValueError("Alert High must be −55 to +150 °C")
            self.safe_write(f"SENSor:ALERt:HIGH {val:.4f}")
        except ValueError as e:
            messagebox.showerror("Validation Error", str(e))

    def _get_alert_high(self):
        resp = self.safe_query("SENSor:ALERt:HIGH?")
        if resp is not None:
            val = SCPIDevice.parse_float(resp)
            if val is not None:
                self._alert_high_entry.delete(0, tk.END)
                self._alert_high_entry.insert(0, f"{val:.4f}")
                self._alert_high_cur.config(text=f"{val:.4f} °C")

    def _set_alert_low(self):
        try:
            val = float(self._alert_low_entry.get())
            if not (-55.0 <= val <= 150.0):
                raise ValueError("Alert Low must be −55 to +150 °C")
            self.safe_write(f"SENSor:ALERt:LOW {val:.4f}")
        except ValueError as e:
            messagebox.showerror("Validation Error", str(e))

    def _get_alert_low(self):
        resp = self.safe_query("SENSor:ALERt:LOW?")
        if resp is not None:
            val = SCPIDevice.parse_float(resp)
            if val is not None:
                self._alert_low_entry.delete(0, tk.END)
                self._alert_low_entry.insert(0, f"{val:.4f}")
                self._alert_low_cur.config(text=f"{val:.4f} °C")

    def _get_alert_status(self):
        resp = self.safe_query("SENSor:ALERt:STATus?")
        if resp is not None:
            cleaned = resp.strip('"')
            color = {"HIGH": "#cc2222", "LOW": "#3366cc",
                     "READY": "#22aa44"}.get(cleaned, "#555555")
            self._alert_status_lbl.config(text=cleaned, foreground=color)

    def _set_mode(self):
        self.safe_write(f"SENSor:MODe {self._mode_var.get()}")

    def _get_mode(self):
        resp = self.safe_query("SENSor:MODe?")
        if resp is not None:
            cleaned = resp.strip('"')
            self._mode_var.set(cleaned)

    def _set_conv_rate(self):
        try:
            val = int(self._conv_rate_var.get())
            if not (0 <= val <= 7):
                raise ValueError("Conv Rate must be 0–7")
            self.safe_write(f"SENSor:CONVrate {val}")
        except (ValueError, tk.TclError) as e:
            messagebox.showerror("Validation Error", str(e))

    def _get_conv_rate(self):
        resp = self.safe_query("SENSor:CONVrate?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            if val is not None:
                self._conv_rate_var.set(val)

    def update_sensor_availability(self, sensor_type: Optional[str]):
        if sensor_type == "SHT45":
            self._na_label.config(foreground="#cc3333",
                                  text="⚠ SHT45 detected — TMP117 commands will return error −221.")
        elif sensor_type in ("TMP117", "DUAL"):
            self._na_label.config(foreground="#22aa44",
                                  text=f"✓ {sensor_type} detected — all TMP117 commands available.")
        else:
            self._na_label.config(foreground="#cc8800",
                                  text="These settings apply to TMP117 only.")


# ===========================================================================
# DisplayTab
# ===========================================================================

class DisplayTab(_BaseTab):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._build()

    def _build(self):
        # --- Brightness ---
        br_frame = ttk.LabelFrame(self.frame, text=tr("frame_brightness"))
        self._tr_frame(br_frame, "frame_brightness")
        br_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_br = ttk.Label(br_frame, text=tr("lbl_bright_range"))
        self._tr(lbl_br, "lbl_bright_range")
        lbl_br.grid(row=0, column=0, padx=6, pady=5)
        self._brightness_val_lbl = ttk.Label(br_frame, text="20", width=4, anchor="e")
        self._brightness_scale = ttk.Scale(br_frame, from_=1, to=100, orient=tk.HORIZONTAL,
                                            length=220, command=self._on_brightness_change)
        self._brightness_scale.set(20)
        self._brightness_scale.grid(row=0, column=1, padx=4)
        self._brightness_val_lbl.grid(row=0, column=2, padx=2)
        set_br = ttk.Button(br_frame, text=tr("btn_set"), command=self._set_brightness, width=6)
        get_br = ttk.Button(br_frame, text=tr("btn_get"), command=self._get_brightness, width=6)
        self._tr(set_br, "btn_set"); self._tr(get_br, "btn_get")
        set_br.grid(row=0, column=3, padx=2)
        get_br.grid(row=0, column=4, padx=2)
        self._connected_widgets += [set_br, get_br]

        # --- State ---
        state_frame = ttk.LabelFrame(self.frame, text=tr("frame_disp_state"))
        self._tr_frame(state_frame, "frame_disp_state")
        state_frame.pack(fill=tk.X, pady=(0, 6))
        self._display_state = tk.StringVar(value="1")
        ttk.Radiobutton(state_frame, text="ON",  variable=self._display_state, value="1").grid(row=0, column=0, padx=8)
        ttk.Radiobutton(state_frame, text="OFF", variable=self._display_state, value="0").grid(row=0, column=1, padx=8)
        set_st = ttk.Button(state_frame, text=tr("btn_set_state"), command=self._set_state, width=10)
        get_st = ttk.Button(state_frame, text=tr("btn_get_state"), command=self._get_state, width=10)
        self._tr(set_st, "btn_set_state"); self._tr(get_st, "btn_get_state")
        set_st.grid(row=0, column=2, padx=4, pady=5)
        get_st.grid(row=0, column=3, padx=4)
        self._connected_widgets += [set_st, get_st]

        # --- Source ---
        src_frame = ttk.LabelFrame(self.frame, text=tr("frame_disp_source"))
        self._tr_frame(src_frame, "frame_disp_source")
        src_frame.pack(fill=tk.X, pady=(0, 6))
        self._display_source = tk.StringVar(value="0")
        rb_meas = ttk.Radiobutton(src_frame, text=tr("disp_src_meas"), variable=self._display_source, value="0")
        rb_text = ttk.Radiobutton(src_frame, text=tr("disp_src_text"), variable=self._display_source, value="1")
        self._tr(rb_meas, "disp_src_meas"); self._tr(rb_text, "disp_src_text")
        rb_meas.grid(row=0, column=0, padx=8)
        rb_text.grid(row=0, column=1, padx=8)
        set_src = ttk.Button(src_frame, text=tr("btn_set_source"), command=self._set_source, width=10)
        get_src = ttk.Button(src_frame, text=tr("btn_get_source"), command=self._get_source, width=10)
        self._tr(set_src, "btn_set_source"); self._tr(get_src, "btn_get_source")
        set_src.grid(row=0, column=2, padx=4, pady=5)
        get_src.grid(row=0, column=3, padx=4)
        self._connected_widgets += [set_src, get_src]

        # --- Text ---
        txt_frame = ttk.LabelFrame(self.frame, text=tr("frame_disp_text"))
        self._tr_frame(txt_frame, "frame_disp_text")
        txt_frame.pack(fill=tk.X, pady=(0, 6))
        lbl_txt = ttk.Label(txt_frame, text=tr("lbl_disp_text"))
        self._tr(lbl_txt, "lbl_disp_text")
        lbl_txt.grid(row=0, column=0, padx=6, pady=5)
        self._text_entry = ttk.Entry(txt_frame, width=12)
        self._text_entry.insert(0, "????????")
        self._text_entry.grid(row=0, column=1, padx=4)
        write_txt = ttk.Button(txt_frame, text=tr("btn_write"), command=self._write_text, width=8)
        read_txt  = ttk.Button(txt_frame, text=tr("btn_read"),  command=self._read_text,  width=8)
        self._tr(write_txt, "btn_write"); self._tr(read_txt, "btn_read")
        write_txt.grid(row=0, column=2, padx=2)
        read_txt.grid(row=0, column=3,  padx=2)
        self._connected_widgets += [write_txt, read_txt]
        self._cur_text_lbl = ttk.Label(txt_frame, text="Current: —", font=("Courier New", 10))
        self._cur_text_lbl.grid(row=1, column=0, columnspan=4, padx=6, pady=(0, 4), sticky="w")

        self._sync_ui_state(False)

    def _on_brightness_change(self, val: str):
        self._brightness_val_lbl.config(text=str(int(float(val))))

    def _set_brightness(self):
        try:
            val = int(float(self._brightness_scale.get()))
            if not (0 <= val <= 100):
                raise ValueError("Brightness must be 0–100")
            self.safe_write(f"DISPlay:BRIGhtness {val}")
        except ValueError as e:
            messagebox.showerror("Validation Error", str(e))

    def _get_brightness(self):
        resp = self.safe_query("DISPlay:BRIGhtness?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            if val is not None:
                self._brightness_scale.set(val)
                self._brightness_val_lbl.config(text=str(val))

    def _set_state(self):
        self.safe_write(f"DISPlay:STATe {self._display_state.get()}")

    def _get_state(self):
        resp = self.safe_query("DISPlay:STATe?")
        if resp is not None:
            self._display_state.set(resp)

    def _set_source(self):
        self.safe_write(f"DISPlay:SOURce {self._display_source.get()}")

    def _get_source(self):
        resp = self.safe_query("DISPlay:SOURce?")
        if resp is not None:
            self._display_source.set(resp)

    def _write_text(self):
        text = self._text_entry.get()
        if len(text) > 8:
            messagebox.showerror("Validation Error", "Text must be ≤ 8 characters")
            return
        self.safe_write(f"DISPlay:TEXT '{text}'")

    def _read_text(self):
        resp = self.safe_query("DISPlay:TEXT?")
        if resp is not None:
            self._text_entry.delete(0, tk.END)
            self._text_entry.insert(0, resp)
            self._cur_text_lbl.config(text=f"Current: {resp}")


# ===========================================================================
# SystemTab
# ===========================================================================

class SystemTab(_BaseTab):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._build()

    def _build(self):
        # --- IEEE 488.2 Registers ---
        reg_frame = ttk.LabelFrame(self.frame, text=tr("frame_ieee_regs"))
        self._tr_frame(reg_frame, "frame_ieee_regs")
        reg_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_ese = ttk.Label(reg_frame, text=tr("lbl_ese"))
        self._tr(lbl_ese, "lbl_ese")
        lbl_ese.grid(row=0, column=0, sticky="w", padx=6, pady=3)
        self._ese_entry = ttk.Entry(reg_frame, width=8)
        self._ese_entry.insert(0, "0")
        self._ese_entry.grid(row=0, column=1, padx=4)
        set_ese = ttk.Button(reg_frame, text="Set *ESE", command=self._set_ese, width=10)
        get_ese = ttk.Button(reg_frame, text="Get *ESE?", command=self._get_ese, width=10)
        set_ese.grid(row=0, column=2, padx=2)
        get_ese.grid(row=0, column=3, padx=2)
        self._connected_widgets += [set_ese, get_ese]

        lbl_sre = ttk.Label(reg_frame, text=tr("lbl_sre"))
        self._tr(lbl_sre, "lbl_sre")
        lbl_sre.grid(row=1, column=0, sticky="w", padx=6, pady=3)
        self._sre_entry = ttk.Entry(reg_frame, width=8)
        self._sre_entry.insert(0, "0")
        self._sre_entry.grid(row=1, column=1, padx=4)
        set_sre = ttk.Button(reg_frame, text="Set *SRE", command=self._set_sre, width=10)
        get_sre = ttk.Button(reg_frame, text="Get *SRE?", command=self._get_sre, width=10)
        set_sre.grid(row=1, column=2, padx=2)
        get_sre.grid(row=1, column=3, padx=2)
        self._connected_widgets += [set_sre, get_sre]

        lbl_esr = ttk.Label(reg_frame, text=tr("lbl_esr"))
        self._tr(lbl_esr, "lbl_esr")
        lbl_esr.grid(row=2, column=0, sticky="w", padx=6, pady=3)
        self._esr_lbl = ttk.Label(reg_frame, text="—", width=8, font=("Courier New", 9))
        self._esr_lbl.grid(row=2, column=1, padx=4)
        get_esr = ttk.Button(reg_frame, text="Query *ESR?", command=self._get_esr, width=12)
        get_esr.grid(row=2, column=2, padx=2)
        self._connected_widgets.append(get_esr)

        lbl_stb = ttk.Label(reg_frame, text=tr("lbl_stb"))
        self._tr(lbl_stb, "lbl_stb")
        lbl_stb.grid(row=3, column=0, sticky="w", padx=6, pady=3)
        self._stb_lbl = ttk.Label(reg_frame, text="—", width=8, font=("Courier New", 9))
        self._stb_lbl.grid(row=3, column=1, padx=4)
        get_stb = ttk.Button(reg_frame, text="Query *STB?", command=self._get_stb, width=12)
        get_stb.grid(row=3, column=2, padx=2)
        opc_btn = ttk.Button(reg_frame, text="*OPC?", command=lambda: self.safe_query("*OPC?"), width=8)
        opc_btn.grid(row=3, column=3, padx=2)
        self._connected_widgets += [get_stb, opc_btn]

        # --- Self Test & Error Queue ---
        err_frame = ttk.LabelFrame(self.frame, text=tr("frame_selftest"))
        self._tr_frame(err_frame, "frame_selftest")
        err_frame.pack(fill=tk.X, pady=(0, 6))

        tst_btn = ttk.Button(err_frame, text="*TST?", command=self._run_self_test, width=8)
        tst_btn.grid(row=0, column=0, padx=6, pady=5)
        self._tst_lbl = ttk.Label(err_frame, text="—", width=16, font=("Courier New", 9, "bold"))
        self._tst_lbl.grid(row=0, column=1, padx=4)

        cnt_btn = ttk.Button(err_frame, text=tr("btn_err_count"),  command=self._get_error_count, width=12)
        all_btn = ttk.Button(err_frame, text=tr("btn_read_errors"), command=self._read_error_queue, width=14)
        cls_btn = ttk.Button(err_frame, text=tr("btn_clear_errors"), command=self._clear_errors, width=12)
        self._tr(cnt_btn, "btn_err_count")
        self._tr(all_btn, "btn_read_errors")
        self._tr(cls_btn, "btn_clear_errors")
        cnt_btn.grid(row=1, column=0, padx=6, pady=4)
        all_btn.grid(row=1, column=1, padx=4)
        cls_btn.grid(row=1, column=2, padx=4)
        self._connected_widgets += [tst_btn, cnt_btn, all_btn, cls_btn]

        # --- Device Info ---
        info_frame = ttk.LabelFrame(self.frame, text=tr("frame_sys_info"))
        self._tr_frame(info_frame, "frame_sys_info")
        info_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_scpiver = ttk.Label(info_frame, text=tr("lbl_scpi_ver"))
        self._tr(lbl_scpiver, "lbl_scpi_ver")
        lbl_scpiver.grid(row=0, column=0, sticky="w", padx=6, pady=3)
        self._sysver_lbl = ttk.Label(info_frame, text="—", width=10, font=("Courier New", 9))
        self._sysver_lbl.grid(row=0, column=1, padx=4)
        ver_btn = ttk.Button(info_frame, text="SYST:VER?", command=self._get_sys_version, width=10)
        ver_btn.grid(row=0, column=2, padx=2)
        self._connected_widgets.append(ver_btn)

        self._id_type = tk.StringVar(value="SHORT")
        ttk.Radiobutton(info_frame, text="Short ID", variable=self._id_type, value="SHORT").grid(row=1, column=0, padx=6, pady=3)
        ttk.Radiobutton(info_frame, text="Long ID",  variable=self._id_type, value="LONG" ).grid(row=1, column=1, padx=4)
        id_btn = ttk.Button(info_frame, text="Get SYST:ID?", command=self._get_sys_id, width=12)
        id_btn.grid(row=1, column=2, padx=2)
        self._sysid_lbl = ttk.Label(info_frame, text="—", font=("Courier New", 9))
        self._sysid_lbl.grid(row=1, column=3, padx=4, sticky="w")
        self._connected_widgets.append(id_btn)

        # --- Device Control ---
        ctrl_frame = ttk.LabelFrame(self.frame, text=tr("frame_device_ctrl"))
        self._tr_frame(ctrl_frame, "frame_device_ctrl")
        ctrl_frame.pack(fill=tk.X, pady=(0, 6))

        boot_btn = ttk.Button(ctrl_frame, text=tr("btn_bootloader"), command=self._enter_bootloader)
        self._tr(boot_btn, "btn_bootloader")
        boot_btn.grid(row=0, column=0, padx=8, pady=6, sticky="w")
        rst_btn = ttk.Button(ctrl_frame, text=tr("btn_restart"), command=self._restart_device)
        self._tr(rst_btn, "btn_restart")
        rst_btn.grid(row=1, column=0, padx=8, pady=(0, 6), sticky="w")
        self._connected_widgets += [boot_btn, rst_btn]

        # --- *OPC / *WAI ---
        misc_frame = ttk.LabelFrame(self.frame, text=tr("frame_misc_ieee"))
        self._tr_frame(misc_frame, "frame_misc_ieee")
        misc_frame.pack(fill=tk.X, pady=(0, 6))
        misc_items = [("btn_opc_set", "*OPC"), ("*WAI", "*WAI"), ("*RST", "*RST"), ("*CLS", "*CLS")]
        for col, (text_or_key, cmd) in enumerate(misc_items):
            label = tr(text_or_key) if text_or_key in _LANGS["EN"] else text_or_key
            btn = ttk.Button(misc_frame, text=label, width=16,
                             command=lambda c=cmd: self.safe_write(c))
            if text_or_key in _LANGS["EN"]:
                self._tr(btn, text_or_key)
            btn.grid(row=0, column=col, padx=4, pady=5)
            self._connected_widgets.append(btn)

        self._sync_ui_state(False)

    def _set_ese(self):
        try:
            val = int(self._ese_entry.get())
            if not (0 <= val <= 255):
                raise ValueError("ESE must be 0–255")
            self.safe_write(f"*ESE {val}")
        except ValueError as e:
            messagebox.showerror("Validation Error", str(e))

    def _get_ese(self):
        resp = self.safe_query("*ESE?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            if val is not None:
                self._ese_entry.delete(0, tk.END)
                self._ese_entry.insert(0, str(val))

    def _set_sre(self):
        try:
            val = int(self._sre_entry.get())
            if not (0 <= val <= 255):
                raise ValueError("SRE must be 0–255")
            self.safe_write(f"*SRE {val}")
        except ValueError as e:
            messagebox.showerror("Validation Error", str(e))

    def _get_sre(self):
        resp = self.safe_query("*SRE?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            if val is not None:
                self._sre_entry.delete(0, tk.END)
                self._sre_entry.insert(0, str(val))

    def _get_esr(self):
        resp = self.safe_query("*ESR?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            self._esr_lbl.config(text=str(val) if val is not None else resp)

    def _get_stb(self):
        resp = self.safe_query("*STB?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            self._stb_lbl.config(text=str(val) if val is not None else resp)

    def _run_self_test(self):
        resp = self.safe_query("*TST?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            if val == 0:
                self._tst_lbl.config(text="PASS", foreground="#22aa44")
            elif val is not None:
                self._tst_lbl.config(text=f"FAIL ({val})", foreground="#cc2222")
            else:
                self._tst_lbl.config(text=resp, foreground="#555555")

    def _get_error_count(self):
        resp = self.safe_query("SYSTem:ERRor:COUNt?")
        if resp is not None:
            val = SCPIDevice.parse_int(resp)
            self.log_pane.log(f"  → Pending errors: {val}", "info")

    def _read_error_queue(self):
        errors_found = 0
        while True:
            resp = self.safe_query("SYSTem:ERRor?")
            if resp is None:
                break
            code_str = resp.split(",")[0].strip()
            try:
                if int(code_str) == 0:
                    if errors_found == 0:
                        self.log_pane.log("  → Error queue is empty", "info")
                    break
            except ValueError:
                break
            errors_found += 1
            self.log_pane.log(f"  → [{errors_found}] {resp}", "warn")
        if errors_found:
            self.log_pane.log(f"  → {errors_found} error(s) read and removed from queue", "warn")

    def _clear_errors(self):
        self.safe_write("*CLS")
        self.log_pane.log("  → Status registers and error queue cleared", "info")

    def _get_sys_version(self):
        resp = self.safe_query("SYSTem:VERSion?")
        if resp is not None:
            self._sysver_lbl.config(text=resp)

    def _get_sys_id(self):
        resp = self.safe_query(f"SYSTem:ID? {self._id_type.get()}")
        if resp is not None:
            self._sysid_lbl.config(text=resp)

    def _enter_bootloader(self):
        if messagebox.askyesno("Confirm Bootloader",
                "Device will restart into DFU bootloader mode.\n\nContinue?"):
            self.safe_write("SYSTem:BOOTloader:ENter")
            self.log_pane.log("Device entered bootloader — use DFU Programmer tab to flash.", "warn")
            self._trigger_comm_error()

    def _restart_device(self):
        if messagebox.askyesno("Confirm Restart", "Device will restart. Continue?"):
            self.safe_write("SYSTem:RST")
            self.log_pane.log("Device restarted — reconnecting automatically...", "warn")
            self._trigger_comm_error()


# ===========================================================================
# MeasurementsTab
# ===========================================================================

class MeasurementsTab(_BaseTab):
    def __init__(self, *args, root: tk.Tk, **kwargs):
        super().__init__(*args, **kwargs)
        self._root = root
        self._plot_window = PlotWindow(root, self.state)
        self._build()

    def _build(self):
        left = ttk.Frame(self.frame)
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        right = ttk.Frame(self.frame)
        right.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(8, 0))

        # --- Polling Configuration ---
        poll_frame = ttk.LabelFrame(left, text=tr("frame_poll_cfg"))
        self._tr_frame(poll_frame, "frame_poll_cfg")
        poll_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_int = ttk.Label(poll_frame, text=tr("lbl_interval"))
        self._tr(lbl_int, "lbl_interval")
        lbl_int.grid(row=0, column=0, sticky="w", padx=6, pady=4)
        self._interval_entry = ttk.Entry(poll_frame, width=10)
        self._interval_entry.insert(0, "2.0")
        self._interval_entry.grid(row=0, column=1, padx=4)

        # --- Controls ---
        ctrl_frame = ttk.LabelFrame(left, text=tr("frame_controls"))
        self._tr_frame(ctrl_frame, "frame_controls")
        ctrl_frame.pack(fill=tk.X, pady=(0, 6))

        self._start_btn = ttk.Button(ctrl_frame, text=tr("btn_start_poll"),
                                      command=self._start_polling, width=18)
        self._stop_btn  = ttk.Button(ctrl_frame, text=tr("btn_stop_poll"),
                                      command=self._stop_polling, width=18)
        self._tr(self._start_btn, "btn_start_poll")
        self._tr(self._stop_btn,  "btn_stop_poll")
        self._start_btn.grid(row=0, column=0, padx=6, pady=5)
        self._stop_btn.grid( row=0, column=1, padx=4)
        self._connected_widgets.append(self._start_btn)

        plot_btn  = ttk.Button(ctrl_frame, text=tr("btn_open_plot"), command=self._plot_window.open, width=12)
        clear_btn = ttk.Button(ctrl_frame, text=tr("btn_clear_data"), command=self._clear_data, width=12)
        self._tr(plot_btn, "btn_open_plot"); self._tr(clear_btn, "btn_clear_data")
        plot_btn.grid( row=1, column=0, padx=6, pady=(0, 5))
        clear_btn.grid(row=1, column=1, padx=4)

        # --- Live Readings ---
        live_frame = ttk.LabelFrame(left, text=tr("frame_live"))
        self._tr_frame(live_frame, "frame_live")
        live_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_lt = ttk.Label(live_frame, text=tr("lbl_temperature"))
        self._tr(lbl_lt, "lbl_temperature")
        lbl_lt.grid(row=0, column=0, sticky="w", padx=6, pady=3)
        self._live_temp_lbl = ttk.Label(live_frame, text="—", width=14,
                                         font=("Courier New", 12, "bold"), foreground="#e05533")
        self._live_temp_lbl.grid(row=0, column=1, padx=4)
        ttk.Label(live_frame, text="°C").grid(row=0, column=2)

        lbl_lh = ttk.Label(live_frame, text=tr("lbl_humidity"))
        self._tr(lbl_lh, "lbl_humidity")
        lbl_lh.grid(row=1, column=0, sticky="w", padx=6, pady=3)
        self._live_hum_lbl = ttk.Label(live_frame, text="—", width=14,
                                        font=("Courier New", 12, "bold"), foreground="#3366cc")
        self._live_hum_lbl.grid(row=1, column=1, padx=4)
        ttk.Label(live_frame, text="%RH").grid(row=1, column=2)

        lbl_samp = ttk.Label(live_frame, text=tr("lbl_samples"))
        self._tr(lbl_samp, "lbl_samples")
        lbl_samp.grid(row=2, column=0, sticky="w", padx=6, pady=3)
        self._sample_count_lbl = ttk.Label(live_frame, text="0", font=("Courier New", 10))
        self._sample_count_lbl.grid(row=2, column=1, padx=4)

        # --- CSV Export ---
        csv_frame = ttk.LabelFrame(right, text=tr("frame_csv"))
        self._tr_frame(csv_frame, "frame_csv")
        csv_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_csv = ttk.Label(csv_frame, text=tr("lbl_csv_file"))
        self._tr(lbl_csv, "lbl_csv_file")
        lbl_csv.grid(row=0, column=0, sticky="w", padx=6, pady=4)
        default_csv = f"measurements_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        self._csv_path_entry = ttk.Entry(csv_frame, width=30)
        self._csv_path_entry.insert(0, default_csv)
        self._csv_path_entry.grid(row=0, column=1, padx=4)
        browse_btn = ttk.Button(csv_frame, text=tr("btn_browse"), command=self._browse_csv, width=8)
        self._tr(browse_btn, "btn_browse")
        browse_btn.grid(row=0, column=2, padx=2)

        self._start_csv_btn = ttk.Button(csv_frame, text=tr("btn_start_csv"), command=self._start_csv, width=10)
        self._stop_csv_btn  = ttk.Button(csv_frame, text=tr("btn_stop_csv"),  command=self._stop_csv,  width=10)
        self._tr(self._start_csv_btn, "btn_start_csv")
        self._tr(self._stop_csv_btn,  "btn_stop_csv")
        self._csv_status_lbl = ttk.Label(csv_frame, text="Not logging", foreground="#888888")
        self._start_csv_btn.grid(row=1, column=0, padx=6, pady=4)
        self._stop_csv_btn.grid( row=1, column=1, padx=4)
        self._csv_status_lbl.grid(row=1, column=2, padx=4)

        self._sync_ui_state(False)
        self._stop_btn.config(state="disabled")

    def _start_polling(self):
        if not self.require_connection():
            return
        try:
            interval = float(self._interval_entry.get())
            if interval <= 0:
                raise ValueError("Interval must be positive")
        except ValueError as e:
            messagebox.showerror("Validation Error", str(e))
            return

        self.state.clear_measurements()
        self.state.start_time = time.time()
        self.state.polling = True
        self._start_btn.config(state="disabled")
        self._stop_btn.config(state="normal")
        self.log_pane.log(f"Polling started (every {interval} s)", "ok")
        self._plot_window.open()
        self._poll_tick(interval)

    def _stop_polling(self):
        self.state.polling = False
        if self.state.poll_after_id:
            try:
                self._root.after_cancel(self.state.poll_after_id)
            except Exception:
                pass
            self.state.poll_after_id = None
        self._start_btn.config(state="normal")
        self._stop_btn.config(state="disabled")
        self.log_pane.log("Polling stopped", "warn")

    def _poll_tick(self, interval: float):
        if not self.state.polling:
            return
        if not self.device.is_connected():
            self._stop_polling()
            return

        temp_val = hum_val = None

        resp = self.safe_query("SENSor:TEMPerature?")
        if not self.device.is_connected():
            self._stop_polling()
            return
        if resp is not None:
            temp_val = SCPIDevice.parse_float(resp)

        if self.state.sensor_type in ("SHT45", "DUAL"):
            resp_h = self.safe_query("SENSor:HUMidity?")
            if resp_h is not None:
                hum_val = SCPIDevice.parse_float(resp_h)

        if self.state.start_time is None:
            self.state.start_time = time.time()
        ts = time.time() - self.state.start_time
        if temp_val is not None:
            self.state.timestamps.append(ts)
            self.state.temp_values.append(temp_val)
        if hum_val is not None:
            self.state.hum_values.append(hum_val)

        self._update_live_labels(temp_val, hum_val)
        self._sample_count_lbl.config(text=str(len(self.state.temp_values)))
        self._plot_window.update()

        if self.state.csv_writer and temp_val is not None:
            ts_iso = datetime.now().isoformat(timespec="milliseconds")
            self.state.csv_writer.writerow([
                ts_iso, f"{ts:.3f}",
                f"{temp_val:.6f}",
                f"{hum_val:.4f}" if hum_val is not None else "",
            ])
            self.state.csv_file.flush()

        self.state.poll_after_id = self._root.after(
            int(interval * 1000), lambda: self._poll_tick(interval))

    def _update_live_labels(self, temp: Optional[float], hum: Optional[float]):
        self._live_temp_lbl.config(text=f"{temp:.4f}" if temp is not None else "N/A")
        self._live_hum_lbl.config(text=f"{hum:.2f}"   if hum  is not None else "N/A")

    def _clear_data(self):
        self.state.clear_measurements()
        self._sample_count_lbl.config(text="0")
        self._live_temp_lbl.config(text="—")
        self._live_hum_lbl.config(text="—")
        self._plot_window.update()
        self.log_pane.log("Measurement data cleared", "info")

    def _browse_csv(self):
        path = filedialog.asksaveasfilename(
            defaultextension=".csv",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
            title="Save measurements to CSV",
        )
        if path:
            self._csv_path_entry.delete(0, tk.END)
            self._csv_path_entry.insert(0, path)

    def _start_csv(self):
        path = self._csv_path_entry.get().strip()
        if not path:
            messagebox.showerror("Error", "Specify a CSV file path first")
            return
        try:
            self.state.csv_file = open(path, "w", newline="", encoding="utf-8")
            self.state.csv_writer = csv.writer(self.state.csv_file)
            self.state.csv_writer.writerow(["timestamp_iso", "elapsed_s",
                                            "temperature_c", "humidity_pct"])
            self._csv_status_lbl.config(text=f"Logging → {path}", foreground="#22aa44")
            self.log_pane.log(f"CSV logging started: {path}", "ok")
        except Exception as e:
            messagebox.showerror("CSV Error", str(e))

    def _stop_csv(self):
        if self.state.csv_file:
            try:
                self.state.csv_file.close()
            except Exception:
                pass
            self.state.csv_file = None
            self.state.csv_writer = None
            self._csv_status_lbl.config(text="Not logging", foreground="#888888")
            self.log_pane.log("CSV logging stopped", "info")

    def stop_polling_if_active(self):
        if self.state.polling:
            self._stop_polling()


# ===========================================================================
# CalibrationTab
# ===========================================================================

class CalibrationTab(_BaseTab):
    """Polynomial calibration wizard.

    Workflow:
      1. Enter pairs (T_measured, T_reference) — minimum 2 points.
      2. Click "Fit polynomial" — numpy fits degree-1..3 polynomial.
      3. Inspect residuals chart and statistics.
      4. Click "Send to device" — writes coefficients via SCPI CAL: subsystem.
    """

    MAX_DEGREE = 5

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._points: list[tuple[float, float]] = []   # (T_raw, T_ref)
        self._coeffs: Optional[list[float]] = None     # [a0..a3] current fit
        self._fig = None
        self._canvas = None
        self._build()

    def _build(self):
        # ---- Top: import numpy check ----
        try:
            import numpy  # noqa: F401
        except ImportError:
            ttk.Label(self.frame,
                      text="numpy is required for calibration fitting.\n"
                           "Install with:  pip install numpy",
                      foreground="#cc3333", font=("Courier New", 10)).pack(pady=20)
            return

        top = ttk.Frame(self.frame)
        top.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)
        left = ttk.Frame(top)
        left.pack(side=tk.LEFT, fill=tk.BOTH)
        right = ttk.Frame(top)
        right.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(10, 0))

        # ---- Point entry ----
        entry_frame = ttk.LabelFrame(left, text=tr("frame_cal_points"))
        self._tr_frame(entry_frame, "frame_cal_points")
        entry_frame.pack(fill=tk.X, pady=(0, 6))

        row0 = ttk.Frame(entry_frame)
        row0.pack(fill=tk.X, padx=6, pady=4)
        lbl_tm = ttk.Label(row0, text=tr("lbl_t_measured"))
        self._tr(lbl_tm, "lbl_t_measured")
        lbl_tm.grid(row=0, column=0, sticky="w")
        self._meas_entry = ttk.Entry(row0, width=12)
        self._meas_entry.grid(row=0, column=1, padx=4)
        lbl_tr = ttk.Label(row0, text=tr("lbl_t_reference"))
        self._tr(lbl_tr, "lbl_t_reference")
        lbl_tr.grid(row=0, column=2, sticky="w", padx=(8, 0))
        self._ref_entry = ttk.Entry(row0, width=12)
        self._ref_entry.grid(row=0, column=3, padx=4)
        add_btn = ttk.Button(row0, text=tr("btn_add_point"), command=self._add_point)
        self._tr(add_btn, "btn_add_point")
        add_btn.grid(row=0, column=4, padx=4)

        self._point_list = tk.Listbox(entry_frame, height=8, width=36,
                                      selectmode=tk.SINGLE, font=("Courier New", 9))
        self._point_list.pack(fill=tk.X, padx=6, pady=(0, 4))
        rem_btn = ttk.Button(entry_frame, text=tr("btn_remove_point"), command=self._remove_point)
        self._tr(rem_btn, "btn_remove_point")
        rem_btn.pack(anchor="w", padx=6, pady=(0, 4))

        # ---- Polynomial degree ----
        deg_frame = ttk.LabelFrame(left, text=tr("frame_cal_degree"))
        self._tr_frame(deg_frame, "frame_cal_degree")
        deg_frame.pack(fill=tk.X, pady=(0, 6))
        self._degree_var = tk.IntVar(value=3)
        inner = ttk.Frame(deg_frame)
        inner.pack(padx=6, pady=4)
        for d in range(1, self.MAX_DEGREE + 1):
            ttk.Radiobutton(inner, text=str(d), variable=self._degree_var,
                            value=d).grid(row=0, column=d - 1, padx=6)

        # ---- Actions ----
        act_frame = ttk.LabelFrame(left, text=tr("frame_cal_actions"))
        self._tr_frame(act_frame, "frame_cal_actions")
        act_frame.pack(fill=tk.X, pady=(0, 6))
        fit_btn = ttk.Button(act_frame, text=tr("btn_fit"), command=self._fit)
        self._tr(fit_btn, "btn_fit")
        fit_btn.pack(fill=tk.X, padx=6, pady=3)
        self._send_btn = ttk.Button(act_frame, text=tr("btn_send_cal"),
                                     command=self._send_to_device, state="disabled")
        self._tr(self._send_btn, "btn_send_cal")
        self._send_btn.pack(fill=tk.X, padx=6, pady=3)
        self._connected_widgets.append(self._send_btn)

        self._read_btn = ttk.Button(act_frame, text=tr("btn_read_cal"),
                                     command=self._read_from_device)
        self._tr(self._read_btn, "btn_read_cal")
        self._read_btn.pack(fill=tk.X, padx=6, pady=3)
        self._connected_widgets.append(self._read_btn)

        self._reset_btn = ttk.Button(act_frame, text=tr("btn_reset_cal"),
                                      command=self._reset_on_device)
        self._tr(self._reset_btn, "btn_reset_cal")
        self._reset_btn.pack(fill=tk.X, padx=6, pady=3)
        self._connected_widgets.append(self._reset_btn)

        # ---- Result label ----
        self._result_lbl = ttk.Label(left, text="", font=("Courier New", 9),
                                     foreground="#226699", wraplength=300, justify="left")
        self._result_lbl.pack(anchor="w", padx=6)

        # ---- Residuals plot ----
        plot_frame = ttk.LabelFrame(right, text=tr("frame_cal_residuals"))
        self._tr_frame(plot_frame, "frame_cal_residuals")
        plot_frame.pack(fill=tk.BOTH, expand=True)
        self._plot_frame = plot_frame

        self._sync_ui_state(False)

    # ------------------------------------------------------------------ #

    def _add_point(self):
        try:
            tm = float(self._meas_entry.get().replace(",", "."))
            tr = float(self._ref_entry.get().replace(",", "."))
        except ValueError:
            messagebox.showerror("Input error", "Enter valid numbers for both temperatures")
            return
        self._points.append((tm, tr))
        self._point_list.insert(tk.END, f"{tm:9.4f}  →  {tr:9.4f}")
        self._meas_entry.delete(0, tk.END)
        self._ref_entry.delete(0, tk.END)
        self._meas_entry.focus()
        self._coeffs = None
        self._send_btn.config(state="disabled")
        self._result_lbl.config(text="")

    def _remove_point(self):
        sel = self._point_list.curselection()
        if not sel:
            return
        idx = sel[0]
        self._point_list.delete(idx)
        self._points.pop(idx)
        self._coeffs = None
        self._send_btn.config(state="disabled")

    def _fit(self):
        import numpy as np

        if len(self._points) < 2:
            messagebox.showerror("Not enough points",
                                 "Add at least 2 calibration points before fitting.")
            return
        degree = self._degree_var.get()
        if len(self._points) <= degree:
            messagebox.showerror("Not enough points",
                                 f"Degree-{degree} fit needs at least {degree + 1} points.")
            return

        t_raw = np.array([p[0] for p in self._points])
        t_ref = np.array([p[1] for p in self._points])

        # Fit T_ref = a0 + a1*T + a2*T^2 + a3*T^3 (polynomial of raw temp)
        coeffs_np = np.polyfit(t_raw, t_ref, deg=degree)
        # np.polyfit returns [a_n, ..., a_1, a_0] (highest degree first)
        coeffs_np = coeffs_np[::-1]  # reverse to [a0, a1, a2, a3]

        # Pad to 6 coefficients (a0..a5)
        full = list(coeffs_np) + [0.0] * (6 - len(coeffs_np))
        self._coeffs = full[:6]

        # Compute calibrated temperatures and residuals
        def poly(t, c):
            return c[0] + c[1]*t + c[2]*t**2 + c[3]*t**3 + c[4]*t**4 + c[5]*t**5

        t_cal = np.array([poly(t, self._coeffs) for t in t_raw])
        residuals = t_cal - t_ref
        rms = float(np.sqrt(np.mean(residuals**2)))
        max_err = float(np.max(np.abs(residuals)))

        # Display result — skip trailing zero coefficients in label
        last_nonzero = max((i for i, v in enumerate(self._coeffs) if abs(v) > 1e-30),
                           default=1)
        coeff_str = "\n".join(
            f"  a{i} = {v:+.6e}" for i, v in enumerate(self._coeffs[:last_nonzero + 1]))
        self._result_lbl.config(
            text=f"Fitted coefficients (deg {degree}):\n{coeff_str}\n\n"
                 f"RMS residual: {rms*1000:.2f} m°C\n"
                 f"Max |error|:  {max_err*1000:.2f} m°C")

        self._send_btn.config(state="normal" if self.device.is_connected() else "disabled")
        self._draw_residuals(t_raw, t_ref, residuals)

    def _draw_residuals(self, t_raw, t_ref, residuals):
        import numpy as np
        import matplotlib
        matplotlib.use("TkAgg")
        import matplotlib.pyplot as plt
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

        for w in self._plot_frame.winfo_children():
            w.destroy()

        res_mc = residuals * 1000  # convert to m°C
        max_abs = float(np.max(np.abs(res_mc)))

        # Ensure y-axis always shows a readable range (minimum ±0.5 m°C)
        y_margin = max(max_abs * 1.4, 0.5)

        fig, ax = plt.subplots(figsize=(5, 3), tight_layout=True)
        ax.axhline(0, color="gray", linewidth=0.8, linestyle="--")
        ax.scatter(t_ref, res_mc, color="#226699", zorder=3, s=50)
        ax.set_ylim(-y_margin, y_margin)
        ax.set_xlabel("T reference [°C]")
        ax.set_ylabel("Residual [m°C]")
        ax.set_title(f"Calibration residuals  (max {max_abs:.3f} m°C)")
        ax.grid(True, alpha=0.3)
        ax.yaxis.set_major_formatter(plt.FormatStrFormatter("%.3f"))

        canvas = FigureCanvasTkAgg(fig, master=self._plot_frame)
        canvas.draw()
        canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        plt.close(fig)

    def _send_to_device(self):
        if self._coeffs is None:
            messagebox.showerror("No fit", "Fit the polynomial first.")
            return
        a0, a1, a2, a3, a4, a5 = self._coeffs
        cmd = f"CAL:COEF {a0:.8e},{a1:.8e},{a2:.8e},{a3:.8e},{a4:.8e},{a5:.8e}"
        if not self.safe_write(cmd):
            return
        self.log_pane.log("Calibration coefficients sent to device", "ok")

    def _read_from_device(self):
        resp = self.safe_query("CAL:COEF?")
        if resp is None:
            return
        state_resp = self.safe_query("CAL:STAT?")
        date_resp  = self.safe_query("CAL:DATE?")
        parts = [p.strip() for p in resp.split(",")]
        if len(parts) == 6:
            try:
                self._coeffs = [float(p) for p in parts]
                self._send_btn.config(state="normal")
                coeff_str = "\n".join(
                    f"  a{i} = {v:+.6e}" for i, v in enumerate(self._coeffs))
                active_str = f"\nactive = {state_resp}" if state_resp else ""
                date_str   = f"\ndate   = {date_resp}"  if date_resp  else ""
                self._result_lbl.config(
                    text=f"Coefficients from device:\n{coeff_str}{active_str}{date_str}")
            except ValueError:
                self.log_pane.log(f"Failed to parse CAL:COEF? response: {resp}", "error")
        else:
            self.log_pane.log(f"Unexpected CAL:COEF? response: {resp}", "error")

    def _reset_on_device(self):
        if not self.safe_write("CAL:RES"):
            return
        self._coeffs = [0.0, 1.0, 0.0, 0.0, 0.0, 0.0]
        self._send_btn.config(state="disabled")
        self._result_lbl.config(
            text="Reset to identity:\n  a0 = +0.0  a1 = +1.0\n  a2..a5 = 0.0\n"
                 "(no temperature correction active)")
        self.log_pane.log("Calibration reset to identity on device", "ok")


# ===========================================================================
# DFUTab
# ===========================================================================

class DFUTab(_BaseTab):
    def __init__(self, *args, root: tk.Tk, **kwargs):
        super().__init__(*args, **kwargs)
        self._root = root
        self._detected_tool: Optional[str] = None
        self._build()

    def _build(self):
        # --- Firmware File ---
        file_frame = ttk.LabelFrame(self.frame, text=tr("frame_fw_file"))
        self._tr_frame(file_frame, "frame_fw_file")
        file_frame.pack(fill=tk.X, pady=(0, 6))

        self._file_entry = ttk.Entry(file_frame, width=52)
        self._file_entry.grid(row=0, column=0, padx=6, pady=6)
        browse_btn = ttk.Button(file_frame, text=tr("btn_browse"), command=self._browse_file, width=8)
        self._tr(browse_btn, "btn_browse")
        browse_btn.grid(row=0, column=1, padx=4)

        # --- Tool Detection ---
        tool_frame = ttk.LabelFrame(self.frame, text=tr("frame_flash_tool"))
        self._tr_frame(tool_frame, "frame_flash_tool")
        tool_frame.pack(fill=tk.X, pady=(0, 6))

        self._tool_lbl = ttk.Label(tool_frame, text="Detecting…", anchor="w")
        self._tool_lbl.grid(row=0, column=0, padx=8, pady=4, sticky="w")
        detect_btn = ttk.Button(tool_frame, text=tr("btn_detect_tool"),
                                command=self._detect_and_show, width=14)
        self._tr(detect_btn, "btn_detect_tool")
        detect_btn.grid(row=0, column=1, padx=4)

        self._tool_help_lbl = ttk.Label(
            tool_frame,
            text="Install dfu-util (https://dfu-util.sourceforge.net) or STM32CubeProgrammer\n"
                 "and ensure the executable is in PATH.",
            foreground="#cc8800",
            justify="left",
        )

        # --- Flash Controls ---
        ctrl_frame = ttk.LabelFrame(self.frame, text=tr("frame_flash_ctrl"))
        self._tr_frame(ctrl_frame, "frame_flash_ctrl")
        ctrl_frame.pack(fill=tk.X, pady=(0, 6))

        self._flash_btn = ttk.Button(ctrl_frame, text=tr("btn_flash"),
                                      command=self._on_flash_click, width=20)
        self._tr(self._flash_btn, "btn_flash")
        self._flash_btn.grid(row=0, column=0, padx=8, pady=8)
        self._flash_step_lbl = ttk.Label(ctrl_frame, text="Idle", anchor="w", width=40)
        self._flash_step_lbl.grid(row=0, column=1, padx=4)

        self._progress = ttk.Progressbar(ctrl_frame, mode="indeterminate", length=300)
        self._progress.grid(row=1, column=0, columnspan=2, padx=8, pady=(0, 6))

        info_lbl = ttk.Label(
            ctrl_frame,
            text="The button sends SYSTem:BOOTloader:ENter, waits 3 s for DFU enumeration,\n"
                 "then flashes the selected file. The device disconnects automatically.",
            justify="left",
            foreground="#555555",
        )
        info_lbl.grid(row=2, column=0, columnspan=2, padx=8, pady=(0, 6), sticky="w")

        # --- Flash Log ---
        log_frame = ttk.LabelFrame(self.frame, text=tr("frame_flash_log"))
        self._tr_frame(log_frame, "frame_flash_log")
        log_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 6))

        self._flash_log = scrolledtext.ScrolledText(
            log_frame, height=10, font=("Courier New", 9), state="disabled")
        self._flash_log.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        clr_btn = ttk.Button(log_frame, text=tr("btn_clear_log"), command=self._clear_flash_log)
        self._tr(clr_btn, "btn_clear_log")
        clr_btn.pack(anchor="e", padx=4, pady=(0, 4))

        self._detect_and_show()

    def _browse_file(self):
        path = filedialog.askopenfilename(
            filetypes=[("Firmware files", "*.hex *.elf"), ("All files", "*.*")],
            title="Select firmware file",
        )
        if path:
            self._file_entry.delete(0, tk.END)
            self._file_entry.insert(0, path)
            self._update_flash_btn_state()

    def _detect_and_show(self):
        tool, path = self._detect_dfu_tool()
        self._detected_tool = tool
        if tool:
            self._tool_lbl.config(text=f"✓ Detected: {tool}  ({path})", foreground="#22aa44")
            self._tool_help_lbl.grid_remove()
        else:
            self._tool_lbl.config(text="✗ No DFU tool found in PATH", foreground="#cc3333")
            self._tool_help_lbl.grid(row=1, column=0, columnspan=2, padx=8, pady=(0, 4), sticky="w")
        self._update_flash_btn_state()

    def _detect_dfu_tool(self):
        for name, probe_cmd in DFU_TOOL_CANDIDATES:
            tool_path = shutil.which(probe_cmd[0])
            if tool_path is None:
                continue
            try:
                r = subprocess.run(probe_cmd, capture_output=True, text=True, timeout=4)
                combined = (r.stdout + r.stderr).lower()
                if r.returncode == 0 or "version" in combined or "dfu" in combined:
                    return name, tool_path
            except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
                continue
        return None, None

    def _update_flash_btn_state(self):
        file_ok = bool(self._file_entry.get().strip())
        tool_ok = self._detected_tool is not None
        self._flash_btn.config(state="normal" if (file_ok and tool_ok) else "disabled")

    def _flash_log_append(self, text: str):
        def _do():
            self._flash_log.config(state="normal")
            self._flash_log.insert(tk.END, text)
            self._flash_log.see(tk.END)
            self._flash_log.config(state="disabled")
        self._root.after(0, _do)

    def _set_step(self, msg: str):
        self._root.after(0, lambda: self._flash_step_lbl.config(text=msg))

    def _clear_flash_log(self):
        self._flash_log.config(state="normal")
        self._flash_log.delete("1.0", tk.END)
        self._flash_log.config(state="disabled")

    def _build_flash_cmd(self, tool: str, file_path: str) -> list:
        if tool == "dfu-util":
            return [
                "dfu-util", "--alt", "0",
                "--dfuse-address", "0x08000000",
                "--download", file_path,
            ]
        else:
            return [
                "STM32_Programmer_CLI",
                "-c", "port=USB1",
                "-d", file_path,
                "-s 0x08000000",
            ]

    def _on_flash_click(self):
        file_path = self._file_entry.get().strip()
        if not file_path:
            messagebox.showerror("Error", "Select a firmware file first")
            return
        if not self._detected_tool:
            messagebox.showerror("Error", "No DFU tool detected")
            return
        if not self.device.is_connected():
            if not messagebox.askyesno(
                "Device Not Connected",
                "Device is not connected via VISA.\n\n"
                "If it is already in DFU mode (e.g. from a previous flash), "
                "you can still attempt to flash.\n\nContinue anyway?"
            ):
                return
        if messagebox.askyesno(
            "Confirm Flash",
            f"File: {file_path}\nTool: {self._detected_tool}\n\n"
            "This will restart the device into DFU mode and flash the firmware. Continue?"
        ):
            self._flash_btn.config(state="disabled")
            self._progress.start(10)
            t = threading.Thread(target=self._flash_thread,
                                 args=(file_path,), daemon=True)
            self.state.dfu_thread = t
            t.start()

    def _flash_thread(self, file_path: str):
        tool = self._detected_tool
        try:
            if self.device.is_connected():
                self._set_step("Sending SYSTem:BOOTloader:ENter…")
                self._flash_log_append("[INFO] Sending bootloader command to device\n")
                try:
                    self.device.write("SYSTem:BOOTloader:ENter")
                except Exception as e:
                    self._flash_log_append(f"[WARN] Command failed (device may have reset): {e}\n")
                self._trigger_comm_error()

            self._set_step(f"Waiting {DFU_WAIT_SECONDS}s for DFU enumeration…")
            self._flash_log_append(f"[INFO] Waiting {DFU_WAIT_SECONDS}s for DFU device to enumerate\n")
            time.sleep(DFU_WAIT_SECONDS)

            cmd = self._build_flash_cmd(tool, file_path)
            self._flash_log_append(f"[CMD] {' '.join(cmd)}\n")
            self._set_step("Flashing…")

            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            for line in proc.stdout:
                self._flash_log_append(line)
            proc.wait()

            if proc.returncode == 0:
                self._set_step("Done — Flash successful!")
                self._flash_log_append("[OK] Flash completed successfully.\n")
                self._root.after(0, lambda: self.log_pane.log(
                    "DFU flash successful — reconnect device", "ok"))
            else:
                self._set_step(f"Failed (exit code {proc.returncode})")
                self._flash_log_append(f"[ERROR] Flash tool exited with code {proc.returncode}\n")
                self._root.after(0, lambda: self.log_pane.log(
                    f"DFU flash failed (code {proc.returncode})", "error"))

        except FileNotFoundError:
            self._set_step("Failed — tool not found")
            self._flash_log_append(f"[ERROR] Tool not found: {cmd[0]}\n")
        except Exception as e:
            self._set_step(f"Error: {e}")
            self._flash_log_append(f"[ERROR] {e}\n")
        finally:
            self._root.after(0, self._progress.stop)
            self._root.after(0, lambda: self._flash_btn.config(state="normal"))


# ===========================================================================
# ConsoleTab
# ===========================================================================

class ConsoleTab(_BaseTab):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._history: List[str] = []
        self._history_idx: int = -1
        self._build()

    def _build(self):
        cmd_frame = ttk.LabelFrame(self.frame, text=tr("frame_scpi_cmd"))
        self._tr_frame(cmd_frame, "frame_scpi_cmd")
        cmd_frame.pack(fill=tk.X, pady=(0, 6))

        lbl_cmd = ttk.Label(cmd_frame, text=tr("lbl_command"))
        self._tr(lbl_cmd, "lbl_command")
        lbl_cmd.grid(row=0, column=0, padx=6, pady=6)
        self._cmd_entry = ttk.Entry(cmd_frame, width=52, font=("Courier New", 10))
        self._cmd_entry.grid(row=0, column=1, padx=4)
        self._cmd_entry.bind("<Return>", self._on_send)
        self._cmd_entry.bind("<Up>",    self._history_up)
        self._cmd_entry.bind("<Down>",  self._history_down)

        send_btn = ttk.Button(cmd_frame, text=tr("btn_send"), command=self._on_send, width=8)
        self._tr(send_btn, "btn_send")
        send_btn.grid(row=0, column=2, padx=4)
        clr_btn = ttk.Button(cmd_frame, text=tr("btn_clear_output"), command=self._clear_output, width=12)
        self._tr(clr_btn, "btn_clear_output")
        clr_btn.grid(row=0, column=3, padx=4)
        self._connected_widgets.append(send_btn)

        hint_lbl = ttk.Label(cmd_frame, text=tr("console_hint"), foreground="#888888")
        self._tr(hint_lbl, "console_hint")
        hint_lbl.grid(row=1, column=0, columnspan=4, padx=6, pady=(0, 4), sticky="w")

        out_frame = ttk.LabelFrame(self.frame, text=tr("frame_response"))
        self._tr_frame(out_frame, "frame_response")
        out_frame.pack(fill=tk.BOTH, expand=True)

        self._output = scrolledtext.ScrolledText(
            out_frame, height=20, font=("Courier New", 9), state="disabled")
        self._output.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        self._output.tag_configure("sent",  foreground="#3366cc")
        self._output.tag_configure("recv",  foreground="#22aa44")
        self._output.tag_configure("error", foreground="#cc2222")
        self._output.tag_configure("info",  foreground="#888888")

        self._sync_ui_state(False)

    def _on_send(self, event=None):
        if not self.require_connection():
            return
        cmd = self._cmd_entry.get().strip()
        if not cmd:
            return
        self._history.append(cmd)
        self._history_idx = -1
        self._cmd_entry.delete(0, tk.END)

        self._append_output(f">>> {cmd}\n", "sent")
        is_query = cmd.endswith("?")
        try:
            if is_query:
                resp = self.device.query(cmd)
                self._append_output(f"    {resp}\n", "recv")
            else:
                self.device.write(cmd)
                self._append_output("    [write OK]\n", "recv")
            self.status_bar.set_last_cmd(cmd)
        except Exception as e:
            self._append_output(f"    ERROR: {e}\n", "error")
            self.log_pane.log(f"Console ERROR [{cmd}]: {e}", "error")

    def _history_up(self, event):
        if not self._history:
            return "break"
        if self._history_idx < len(self._history) - 1:
            self._history_idx += 1
        self._cmd_entry.delete(0, tk.END)
        self._cmd_entry.insert(0, self._history[-(self._history_idx + 1)])
        return "break"

    def _history_down(self, event):
        if self._history_idx <= 0:
            self._history_idx = -1
            self._cmd_entry.delete(0, tk.END)
            return "break"
        self._history_idx -= 1
        self._cmd_entry.delete(0, tk.END)
        self._cmd_entry.insert(0, self._history[-(self._history_idx + 1)])
        return "break"

    def _append_output(self, text: str, tag: str = "info"):
        self._output.config(state="normal")
        self._output.insert(tk.END, text, tag)
        self._output.see(tk.END)
        self._output.config(state="disabled")

    def _clear_output(self):
        self._output.config(state="normal")
        self._output.delete("1.0", tk.END)
        self._output.config(state="disabled")


# ===========================================================================
# ConfigTab — persistent configuration management (SAVE / RESTORE / RECALL)
# ===========================================================================

class ConfigTab(_BaseTab):
    """Three-tier configuration storage: DEFAULT / PRIMARY / BACKUP."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._dirty_var = tk.StringVar(value="—")
        self._build()

    def _build(self):
        # ── Information section ────────────────────────────────────────────
        info_frame = ttk.LabelFrame(self.frame, text=tr("cfg_title"))
        self._tr_frame(info_frame, "cfg_title")
        info_frame.pack(fill=tk.X, pady=(0, 8))

        self._cfg_info_lbl = ttk.Label(info_frame, text=tr("cfg_info_text"),
                                       justify=tk.LEFT, foreground="#444444")
        self._tr(self._cfg_info_lbl, "cfg_info_text")
        self._cfg_info_lbl.pack(anchor="w", padx=8, pady=6)

        # ── Dirty indicator ────────────────────────────────────────────────
        status_frame = ttk.LabelFrame(self.frame, text=tr("cfg_status_frame"))
        self._tr_frame(status_frame, "cfg_status_frame")
        status_frame.pack(fill=tk.X, pady=(0, 8))

        status_inner = ttk.Frame(status_frame)
        status_inner.pack(fill=tk.X, padx=8, pady=6)

        dirty_lbl = ttk.Label(status_inner, text=tr("cfg_dirty_label"))
        self._tr(dirty_lbl, "cfg_dirty_label")
        dirty_lbl.pack(side=tk.LEFT)
        self._dirty_label = ttk.Label(status_inner, textvariable=self._dirty_var,
                                      font=("", 10, "bold"), foreground="#888888", width=6)
        self._dirty_label.pack(side=tk.LEFT, padx=(6, 16))

        refresh_btn = ttk.Button(status_inner, text=tr("cfg_refresh_btn"), width=10,
                                 command=self._refresh_dirty)
        self._tr(refresh_btn, "cfg_refresh_btn")
        refresh_btn.pack(side=tk.LEFT)
        self._connected_widgets.append(refresh_btn)

        # ── Action buttons ─────────────────────────────────────────────────
        act_frame = ttk.LabelFrame(self.frame, text=tr("cfg_operations"))
        self._tr_frame(act_frame, "cfg_operations")
        act_frame.pack(fill=tk.X, pady=(0, 8))

        btn_grid = ttk.Frame(act_frame)
        btn_grid.pack(fill=tk.X, padx=8, pady=8)

        # SAVE
        save_btn = ttk.Button(btn_grid, text="💾  Save Config",
                              command=self._on_save, width=22)
        save_btn.grid(row=0, column=0, padx=6, pady=4, sticky="w")
        save_desc = ttk.Label(btn_grid, text=tr("cfg_save_desc"), foreground="#555555")
        self._tr(save_desc, "cfg_save_desc")
        save_desc.grid(row=0, column=1, padx=6, sticky="w")
        self._connected_widgets.append(save_btn)

        # RESTORE
        restore_btn = ttk.Button(btn_grid, text="↩  Restore from Backup",
                                 command=self._on_restore, width=22)
        restore_btn.grid(row=1, column=0, padx=6, pady=4, sticky="w")
        restore_desc = ttk.Label(btn_grid, text=tr("cfg_restore_desc"), foreground="#555555")
        self._tr(restore_desc, "cfg_restore_desc")
        restore_desc.grid(row=1, column=1, padx=6, sticky="w")
        self._connected_widgets.append(restore_btn)

        # RECALL
        recall_btn = ttk.Button(btn_grid, text="🏭  Factory Recall",
                                command=self._on_recall, width=22)
        recall_btn.grid(row=2, column=0, padx=6, pady=4, sticky="w")
        recall_desc = ttk.Label(btn_grid, text=tr("cfg_recall_desc"), foreground="#555555")
        self._tr(recall_desc, "cfg_recall_desc")
        recall_desc.grid(row=2, column=1, padx=6, sticky="w")
        self._connected_widgets.append(recall_btn)

        self._sync_ui_state(False)

    # ── Handlers ───────────────────────────────────────────────────────────

    def _refresh_dirty(self):
        if not self.require_connection():
            return
        resp = self.safe_query("SYSTem:CONFig:DIRty?")
        if resp is None:
            return
        dirty = resp.strip() not in ("0", "FALSE", "false")
        self._dirty_var.set(tr("cfg_yes") if dirty else tr("cfg_no"))
        self._dirty_label.config(foreground="#cc3333" if dirty else "#33aa55")

    def _on_save(self):
        if not self.require_connection():
            return
        ok = self.safe_write("SYSTem:CONFig:SAVE")
        if ok:
            self.log_pane.log("Config saved to PRIMARY + BACKUP flash.", "ok")
            self._refresh_dirty()

    def _on_restore(self):
        if not self.require_connection():
            return
        if not messagebox.askyesno("Restore from Backup", tr("cfg_restore_msg")):
            return
        ok = self.safe_write("SYSTem:CONFig:RESTore")
        if ok:
            self.log_pane.log("PRIMARY restored from BACKUP.", "ok")
            self._refresh_dirty()

    def _on_recall(self):
        if not self.require_connection():
            return
        if not messagebox.askyesno("Factory Recall", tr("cfg_recall_msg")):
            return
        ok = self.safe_write("SYSTem:CONFig:RECall")
        if ok:
            self.log_pane.log("Factory defaults recalled to PRIMARY + BACKUP.", "ok")
            self._refresh_dirty()

    def on_tab_selected(self):
        """Called when this tab is brought to focus — auto-refresh dirty flag."""
        if self.device.is_connected():
            self._refresh_dirty()


# ===========================================================================
# SDTApp — orchestrator
# ===========================================================================

class SDTApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("SDT Board Companion")
        self.root.minsize(820, 620)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        self.device = SCPIDevice()
        self.state  = AppState()

        self._build_layout()

    def _build_layout(self):
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        self.log_pane   = LogPane(self.root, height=6)
        self.log_pane.frame.pack(fill=tk.X, padx=6, pady=(0, 2))

        self.status_bar = StatusBar(self.root)
        self.status_bar.frame.pack(fill=tk.X, side=tk.BOTTOM)

        common = dict(state=self.state, device=self.device,
                      log_pane=self.log_pane, status_bar=self.status_bar)

        self.conn_tab    = ConnectionTab(self.notebook, **common)
        self.sensor_tab  = SensorTab(self.notebook, **common)
        self.tmp117_tab  = TMP117Tab(self.notebook, **common)
        self.disp_tab    = DisplayTab(self.notebook, **common)
        self.sys_tab     = SystemTab(self.notebook, **common)
        self.config_tab  = ConfigTab(self.notebook, **common)
        self.meas_tab    = MeasurementsTab(self.notebook, **common, root=self.root)
        self.cal_tab     = CalibrationTab(self.notebook, **common)
        self.dfu_tab     = DFUTab(self.notebook, **common, root=self.root)
        self.console_tab = ConsoleTab(self.notebook, **common)

        tabs = [
            (self.conn_tab,    "Connection"),
            (self.sensor_tab,  "Sensor"),
            (self.tmp117_tab,  "TMP117"),
            (self.disp_tab,    "Display"),
            (self.sys_tab,     "System"),
            (self.config_tab,  "Config"),
            (self.meas_tab,    "Measurements"),
            (self.cal_tab,     "Calibration"),
            (self.dfu_tab,     "DFU Programmer"),
            (self.console_tab, "Console"),
        ]
        for tab, label in tabs:
            self.notebook.add(tab.frame, text=f"  {label}  ")

        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_changed)

        self.conn_tab.on_connect_callback    = self._on_device_connected
        self.conn_tab.on_disconnect_callback = self._on_device_disconnected
        self.conn_tab.on_lang_change         = self._on_all_lang_change

        all_tabs = [self.conn_tab, self.sensor_tab, self.tmp117_tab, self.disp_tab,
                    self.sys_tab, self.config_tab, self.meas_tab,
                    self.cal_tab, self.dfu_tab, self.console_tab]
        for tab in all_tabs:
            tab.on_comm_error = self.conn_tab._handle_external_disconnect

        self.log_pane.log("SDT Board Companion started — select a VISA resource and click Connect.", "info")

    def _on_all_lang_change(self) -> None:
        """Propagate language change to all tabs."""
        all_tabs = [self.conn_tab, self.sensor_tab, self.tmp117_tab, self.disp_tab,
                    self.sys_tab, self.config_tab, self.meas_tab,
                    self.cal_tab, self.dfu_tab, self.console_tab]
        for tab in all_tabs:
            tab.refresh_lang()

    def _on_tab_changed(self, _event=None):
        try:
            idx = self.notebook.index(self.notebook.select())
            tabs_list = [self.conn_tab, self.sensor_tab, self.tmp117_tab,
                         self.disp_tab, self.sys_tab, self.config_tab,
                         self.meas_tab, self.cal_tab, self.dfu_tab, self.console_tab]
            tab = tabs_list[idx]
            if hasattr(tab, "on_tab_selected"):
                tab.on_tab_selected()
        except Exception:
            pass

    def _on_device_connected(self, resource_name: str, sensor_type: Optional[str]):
        self.status_bar.set_connected(resource_name)
        self.status_bar.set_sensor_type(sensor_type)
        self.tmp117_tab.update_sensor_availability(sensor_type)
        self.sensor_tab.update_sensor_availability(sensor_type)
        for tab in (self.sensor_tab, self.tmp117_tab, self.disp_tab,
                    self.sys_tab, self.config_tab, self.meas_tab,
                    self.cal_tab, self.console_tab):
            tab._sync_ui_state(True)

    def _on_device_disconnected(self):
        self.status_bar.set_connected(None)
        self.status_bar.set_sensor_type(None)
        self.meas_tab.stop_polling_if_active()
        for tab in (self.sensor_tab, self.tmp117_tab, self.disp_tab,
                    self.sys_tab, self.config_tab, self.meas_tab,
                    self.cal_tab, self.console_tab):
            tab._sync_ui_state(False)

    def _on_close(self):
        self.meas_tab.stop_polling_if_active()
        self.meas_tab._stop_csv()
        if self._plot_window_open():
            self.meas_tab._plot_window.close()
        self.device.disconnect()
        self.root.destroy()

    def _plot_window_open(self) -> bool:
        try:
            return self.meas_tab._plot_window.is_open
        except Exception:
            return False


# ===========================================================================
# Entry point
# ===========================================================================

if __name__ == "__main__":
    try:
        from ttkthemes import ThemedTk
        root = ThemedTk(theme="arc")
    except ImportError:
        root = tk.Tk()

    root.resizable(True, True)
    app = SDTApp(root)
    root.mainloop()
