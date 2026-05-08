import tkinter as tk
from tkinter import ttk, messagebox
import pyvisa
import csv
import time
import math

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# Conversion rate code → period string (TMP117 datasheet)
CONV_RATE_LABELS = {
    0: "15.5 ms", 1: "125 ms", 2: "250 ms", 3: "500 ms",
    4: "1 s",     5: "4 s",   6: "8 s",   7: "16 s",
}

# TMP117 hardware averaging: accepted values only
TMP117_AVG_VALUES = (1, 8, 32, 64)


class SCPIGui:
    def __init__(self, root):
        self.root = root
        self.root.title("SCPI Device GUI - SDT Board")
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

        self.rm = pyvisa.ResourceManager()
        self.device = None
        self.sensor_type = None  # last known sensor type ("TMP117"/"SHT45"/"DUAL"/None)

        self.polling = False
        self.timestamps = []
        self.values = []
        self.start_time = None

        self.csv_file = open("measurements.csv", "w", newline="")
        self.csv_writer = csv.writer(self.csv_file)
        self.csv_writer.writerow(["timestamp", "type", "value"])

        self.create_widgets()
        self.refresh_resources()
        self.setup_plot()

    # ------------------------------------------------------------------ #
    # Widget creation
    # ------------------------------------------------------------------ #

    def create_widgets(self):
        # --- Connection ---
        conn_frame = ttk.LabelFrame(self.root, text="Connection")
        conn_frame.grid(row=0, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        self.resource_combo = ttk.Combobox(conn_frame, width=40)
        self.resource_combo.grid(row=0, column=0, padx=5, pady=5)
        ttk.Button(conn_frame, text="Refresh",    command=self.refresh_resources).grid(row=0, column=1, padx=2)
        ttk.Button(conn_frame, text="Connect",    command=self.connect).grid(row=0, column=2, padx=2)
        ttk.Button(conn_frame, text="Disconnect", command=self.disconnect).grid(row=0, column=3, padx=2)

        ttk.Button(self.root, text="*IDN?", command=self.get_idn, width=60).grid(row=1, column=0, pady=5)

        # --- Sensor (universal) ---
        sensor_frame = ttk.LabelFrame(self.root, text="Sensor")
        sensor_frame.grid(row=2, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        ttk.Button(sensor_frame, text="Type?",        command=self.get_sensor_type).grid(row=0, column=0, padx=2)
        ttk.Button(sensor_frame, text="Temperature?", command=self.get_temperature).grid(row=0, column=1, padx=2)
        ttk.Button(sensor_frame, text="Humidity?",    command=self.get_humidity).grid(row=0, column=2, padx=2)
        ttk.Button(sensor_frame, text="ID?",          command=self.get_sensor_id).grid(row=0, column=3, padx=2)
        ttk.Button(sensor_frame, text="Heater ON",    command=self.run_heater).grid(row=0, column=4, padx=2)
        ttk.Button(sensor_frame, text="Soft Reset",   command=self.soft_reset).grid(row=0, column=5, padx=2)

        # --- Shared configuration (READperiod, AVErage, PRECision) ---
        config_frame = ttk.LabelFrame(self.root, text="Sensor Configuration")
        config_frame.grid(row=3, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        ttk.Label(config_frame, text="Read Period [ms]:").grid(row=0, column=0, sticky="w")
        self.period_entry = ttk.Entry(config_frame, width=12)
        self.period_entry.insert(0, "500")
        self.period_entry.grid(row=0, column=1, padx=5)
        ttk.Button(config_frame, text="Set", command=self.set_read_period).grid(row=0, column=2, padx=2)
        ttk.Button(config_frame, text="Get", command=self.get_read_period).grid(row=0, column=3, padx=2)

        ttk.Label(config_frame, text="Average (SHT45: 1-255 / TMP117: 1,8,32,64):").grid(row=1, column=0, sticky="w")
        self.average_entry = ttk.Entry(config_frame, width=12)
        self.average_entry.insert(0, "1")
        self.average_entry.grid(row=1, column=1, padx=5)
        ttk.Button(config_frame, text="Set", command=self.set_average).grid(row=1, column=2, padx=2)
        ttk.Button(config_frame, text="Get", command=self.get_average).grid(row=1, column=3, padx=2)

        ttk.Label(config_frame, text="Precision (SHT45 only):").grid(row=2, column=0, sticky="w")
        self.precision_var = tk.StringVar(value="HIGH")
        ttk.Combobox(config_frame, textvariable=self.precision_var,
                     values=["LOW", "MEDIUM", "HIGH"], state="readonly", width=10
                     ).grid(row=2, column=1, padx=5)
        ttk.Button(config_frame, text="Set", command=self.set_precision).grid(row=2, column=2, padx=2)
        ttk.Button(config_frame, text="Get", command=self.get_precision).grid(row=2, column=3, padx=2)

        # --- TMP117 Configuration ---
        tmp_frame = ttk.LabelFrame(self.root, text="TMP117 Configuration")
        tmp_frame.grid(row=4, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        # Alert thresholds
        ttk.Label(tmp_frame, text="Alert High [°C]:").grid(row=0, column=0, sticky="w")
        self.alert_high_entry = ttk.Entry(tmp_frame, width=10)
        self.alert_high_entry.insert(0, "85.0")
        self.alert_high_entry.grid(row=0, column=1, padx=5)
        ttk.Button(tmp_frame, text="Set", command=self.set_alert_high).grid(row=0, column=2, padx=2)
        ttk.Button(tmp_frame, text="Get", command=self.get_alert_high).grid(row=0, column=3, padx=2)

        ttk.Label(tmp_frame, text="Alert Low [°C]:").grid(row=1, column=0, sticky="w")
        self.alert_low_entry = ttk.Entry(tmp_frame, width=10)
        self.alert_low_entry.insert(0, "0.0")
        self.alert_low_entry.grid(row=1, column=1, padx=5)
        ttk.Button(tmp_frame, text="Set", command=self.set_alert_low).grid(row=1, column=2, padx=2)
        ttk.Button(tmp_frame, text="Get", command=self.get_alert_low).grid(row=1, column=3, padx=2)

        ttk.Button(tmp_frame, text="Alert Status?", command=self.get_alert_status).grid(row=0, column=4, padx=4)

        # Conversion mode
        ttk.Label(tmp_frame, text="Mode:").grid(row=2, column=0, sticky="w")
        self.mode_var = tk.StringVar(value="CONTINUOUS")
        ttk.Combobox(tmp_frame, textvariable=self.mode_var,
                     values=["CONTINUOUS", "SHUTDOWN", "ONESHOT"], state="readonly", width=12
                     ).grid(row=2, column=1, padx=5)
        ttk.Button(tmp_frame, text="Set", command=self.set_mode).grid(row=2, column=2, padx=2)
        ttk.Button(tmp_frame, text="Get", command=self.get_mode).grid(row=2, column=3, padx=2)

        # Conversion rate
        ttk.Label(tmp_frame, text="Conv Rate (0-7):").grid(row=3, column=0, sticky="w")
        self.conv_rate_var = tk.IntVar(value=4)
        conv_spin = ttk.Spinbox(tmp_frame, from_=0, to=7, textvariable=self.conv_rate_var, width=5)
        conv_spin.grid(row=3, column=1, padx=5, sticky="w")
        self.conv_rate_label = ttk.Label(tmp_frame, text=f"→ {CONV_RATE_LABELS[4]}", foreground="gray")
        self.conv_rate_label.grid(row=3, column=2, padx=2, sticky="w")
        ttk.Button(tmp_frame, text="Set", command=self.set_conv_rate).grid(row=3, column=3, padx=2)
        ttk.Button(tmp_frame, text="Get", command=self.get_conv_rate).grid(row=3, column=4, padx=2)
        self.conv_rate_var.trace_add("write", self._update_conv_rate_label)

        # --- Cyclic Measurement ---
        cycle_frame = ttk.LabelFrame(self.root, text="Cyclic Measurement")
        cycle_frame.grid(row=5, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        self.measure_type = tk.StringVar(value="TEMP")
        ttk.Radiobutton(cycle_frame, text="Temperature", variable=self.measure_type, value="TEMP").grid(row=0, column=0)
        ttk.Radiobutton(cycle_frame, text="Humidity",    variable=self.measure_type, value="HUM").grid(row=0, column=1)

        ttk.Label(cycle_frame, text="Interval [s]:").grid(row=1, column=0)
        self.interval_entry = ttk.Entry(cycle_frame, width=10)
        self.interval_entry.insert(0, "2")
        self.interval_entry.grid(row=1, column=1)

        ttk.Button(cycle_frame, text="Start",     command=self.start_polling).grid(row=2, column=0, padx=2)
        ttk.Button(cycle_frame, text="Stop",      command=self.stop_polling).grid(row=2, column=1, padx=2)
        ttk.Button(cycle_frame, text="Open Plot", command=self.open_plot_window).grid(row=2, column=2, padx=2)
        ttk.Button(cycle_frame, text="Clear Plot", command=self.clear_plot_window).grid(row=2, column=3, padx=2)

        # --- Display ---
        display_frame = ttk.LabelFrame(self.root, text="Display")
        display_frame.grid(row=6, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        ttk.Label(display_frame, text="Brightness (0-100):").grid(row=0, column=0)
        self.brightness_entry = ttk.Entry(display_frame, width=10)
        self.brightness_entry.insert(0, "5")
        self.brightness_entry.grid(row=0, column=1)
        ttk.Button(display_frame, text="Set", command=self.set_brightness).grid(row=0, column=2, padx=2)
        ttk.Button(display_frame, text="Get", command=self.get_brightness).grid(row=0, column=3, padx=2)

        self.display_state = tk.StringVar(value="1")
        ttk.Radiobutton(display_frame, text="ON",  variable=self.display_state, value="1").grid(row=1, column=0)
        ttk.Radiobutton(display_frame, text="OFF", variable=self.display_state, value="0").grid(row=1, column=1)
        ttk.Button(display_frame, text="Set State", command=self.set_display_state).grid(row=1, column=2, padx=2)
        ttk.Button(display_frame, text="Get State", command=self.get_display_state).grid(row=1, column=3, padx=2)

        self.display_source = tk.StringVar(value="0")
        ttk.Radiobutton(display_frame, text="Meas", variable=self.display_source, value="0").grid(row=2, column=0)
        ttk.Radiobutton(display_frame, text="Text", variable=self.display_source, value="1").grid(row=2, column=1)
        ttk.Button(display_frame, text="Set Source", command=self.set_display_source).grid(row=2, column=2, padx=2)
        ttk.Button(display_frame, text="Get Source", command=self.get_display_source).grid(row=2, column=3, padx=2)

        ttk.Label(display_frame, text="Text (max 8):").grid(row=3, column=0)
        self.text_entry = ttk.Entry(display_frame, width=10)
        self.text_entry.insert(0, "????????")
        self.text_entry.grid(row=3, column=1)
        ttk.Button(display_frame, text="Write", command=self.write_text).grid(row=3, column=2, padx=2)
        ttk.Button(display_frame, text="Read",  command=self.read_text).grid(row=3, column=3, padx=2)

        # --- System ---
        system_frame = ttk.LabelFrame(self.root, text="System")
        system_frame.grid(row=7, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        ttk.Button(system_frame, text="Enter Bootloader", command=self.enter_bootloader).grid(row=0, column=0, padx=2)
        ttk.Button(system_frame, text="Restart",          command=self.restart).grid(row=0, column=1, padx=2)

        self.id_type = tk.StringVar(value="SHORT")
        ttk.Radiobutton(system_frame, text="Short", variable=self.id_type, value="SHORT").grid(row=1, column=0)
        ttk.Radiobutton(system_frame, text="Long",  variable=self.id_type, value="LONG").grid(row=1, column=1)
        ttk.Button(system_frame, text="Get ID", command=self.get_device_id).grid(row=1, column=2, padx=2)

        # --- SCPI Error Queue ---
        err_frame = ttk.LabelFrame(self.root, text="SCPI Error Queue")
        err_frame.grid(row=8, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        ttk.Button(err_frame, text="Count (SYST:ERR:COUN?)",
                   command=self.get_error_count).grid(row=0, column=0, padx=4, pady=3)
        ttk.Button(err_frame, text="Read All Errors",
                   command=self.read_error_queue).grid(row=0, column=1, padx=4)
        ttk.Button(err_frame, text="Clear All (*CLS)",
                   command=self.clear_errors).grid(row=0, column=2, padx=4)
        ttk.Button(err_frame, text="Self-test (*TST?)",
                   command=self.run_self_test).grid(row=0, column=3, padx=4)

        # --- Output log ---
        self.output = tk.Text(self.root, height=8, width=60)
        self.output.grid(row=9, column=0, padx=10, pady=10, sticky="nsew")
        scrollbar = ttk.Scrollbar(self.root, orient="vertical", command=self.output.yview)
        scrollbar.grid(row=9, column=1, sticky="ns")
        self.output.config(yscrollcommand=scrollbar.set)

    def _update_conv_rate_label(self, *_):
        try:
            rate = int(self.conv_rate_var.get())
            self.conv_rate_label.config(text=f"→ {CONV_RATE_LABELS.get(rate, '?')}")
        except (tk.TclError, ValueError):
            self.conv_rate_label.config(text="→ ?")

    # ------------------------------------------------------------------ #
    # Plot
    # ------------------------------------------------------------------ #

    def setup_plot(self):
        self.plot_window = None
        self.fig = self.ax = self.line = self.canvas = None

    def open_plot_window(self):
        if self.plot_window is not None:
            return
        self.plot_window = tk.Toplevel(self.root)
        self.plot_window.title("Live Measurement Plot")
        self.plot_window.protocol("WM_DELETE_WINDOW", self._close_plot)

        self.fig, self.ax = plt.subplots(figsize=(8, 5))
        self.line, = self.ax.plot([], [], "-o", linewidth=2, markersize=4)
        self.ax.set_title("Live Measurement", fontsize=12, fontweight="bold")
        self.ax.set_xlabel("Time [s]")
        self.ax.set_ylabel("Value")
        self.ax.grid(True, alpha=0.3)

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.plot_window)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

    def _close_plot(self):
        if self.plot_window:
            self.plot_window.destroy()
        self.plot_window = self.fig = self.ax = self.line = self.canvas = None

    def clear_plot_window(self):
        self.start_time = time.time()
        self.timestamps.clear()
        self.values.clear()
        self.update_plot()
        self.log("Plot cleared")

    def update_plot(self):
        if self.plot_window is None or self.line is None:
            return
        self.line.set_data(self.timestamps, self.values)
        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas.draw_idle()

    # ------------------------------------------------------------------ #
    # Logging
    # ------------------------------------------------------------------ #

    def log(self, text):
        self.output.insert(tk.END, f"[{time.strftime('%H:%M:%S')}] {text}\n")
        self.output.see(tk.END)

    # ------------------------------------------------------------------ #
    # Connection
    # ------------------------------------------------------------------ #

    def refresh_resources(self):
        try:
            resources = self.rm.list_resources()
            self.resource_combo["values"] = resources
            if resources:
                self.resource_combo.current(0)
            self.log(f"Found {len(resources)} device(s)")
        except Exception as e:
            self.log(f"ERROR: Failed to refresh resources: {e}")

    def connect(self):
        try:
            resource = self.resource_combo.get()
            if not resource:
                messagebox.showerror("Error", "Please select a device")
                return
            self.device = self.rm.open_resource(resource)
            self.device.timeout = 5000
            self.log(f"✓ Connected to {resource}")
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))
            self.log(f"✗ Connection failed: {e}")

    def disconnect(self):
        if not self.device:
            messagebox.showerror("Error", "Not connected")
            return
        try:
            name = self.device.resource_name
            self.device.close()
            self.device = None
            self.sensor_type = None
            self.log(f"✓ Disconnected from {name}")
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))
            self.log(f"✗ Disconnect failed: {e}")

    # ------------------------------------------------------------------ #
    # SCPI transport
    # ------------------------------------------------------------------ #

    def query(self, cmd):
        if not self.device:
            raise ConnectionError("Not connected to device")
        return self.device.query(cmd).strip()

    def write(self, cmd):
        if not self.device:
            raise ConnectionError("Not connected to device")
        self.device.write(cmd)

    def safe_query_log(self, cmd):
        try:
            resp = self.query(cmd)
            self.log(f"✓ {cmd} → {resp}")
            return resp
        except Exception as e:
            self.log(f"✗ ERROR [{cmd}]: {e}")
            self.stop_polling()
            return None

    def safe_write_log(self, cmd):
        try:
            self.write(cmd)
            self.log(f"✓ {cmd}")
        except Exception as e:
            self.log(f"✗ ERROR [{cmd}]: {e}")
            messagebox.showerror("Error", str(e))

    @staticmethod
    def _parse_float(text):
        """Parse float, returning None for SCPI NaN (9.91E+37 or 'nan')."""
        try:
            val = float(text)
            if math.isnan(val) or abs(val) > 9e36:
                return None
            return val
        except (ValueError, TypeError):
            return None

    # ------------------------------------------------------------------ #
    # System
    # ------------------------------------------------------------------ #

    def get_idn(self):
        self.safe_query_log("*IDN?")

    def get_device_id(self):
        self.safe_query_log(f"SYSTem:ID? {self.id_type.get()}")

    def enter_bootloader(self):
        if messagebox.askyesno("Confirm", "Device will restart into bootloader mode. Continue?"):
            self.safe_write_log("SYSTem:BOOTloader:ENter")
            self.device = None

    def restart(self):
        if messagebox.askyesno("Confirm", "Device will restart. Continue?"):
            self.safe_write_log("SYSTem:RST")
            self.device = None

    # ------------------------------------------------------------------ #
    # SCPI Error Queue
    # ------------------------------------------------------------------ #

    def get_error_count(self):
        resp = self.safe_query_log("SYSTem:ERRor:COUNt?")
        if resp:
            try:
                self.log(f"  → Pending errors: {int(resp)}")
            except ValueError:
                pass

    def read_error_queue(self):
        """Read and log all errors until queue empty (0,'No error')."""
        errors_found = 0
        while True:
            resp = self.safe_query_log("SYSTem:ERRor?")
            if resp is None:
                break
            code_str = resp.split(",")[0].strip()
            try:
                if int(code_str) == 0:
                    if errors_found == 0:
                        self.log("  → Error queue is empty")
                    break
            except ValueError:
                break
            errors_found += 1
            self.log(f"  → [{errors_found}] {resp}")
        if errors_found:
            self.log(f"  → {errors_found} error(s) read and removed from queue")

    def clear_errors(self):
        self.safe_write_log("*CLS")
        self.log("  → Status registers and error queue cleared")

    def run_self_test(self):
        resp = self.safe_query_log("*TST?")
        if resp:
            try:
                result = int(resp)
                status = "PASS" if result == 0 else f"FAIL (code {result})"
                self.log(f"  → Self-test result: {status}")
            except ValueError:
                pass

    # ------------------------------------------------------------------ #
    # Sensor — universal
    # ------------------------------------------------------------------ #

    def get_sensor_type(self):
        resp = self.safe_query_log("SENSor:TYPE?")
        if resp:
            self.sensor_type = resp.strip('"')
            self.log(f"  → Sensor: {self.sensor_type}")

    def get_temperature(self):
        resp = self.safe_query_log("SENSor:TEMPerature?")
        if resp:
            val = self._parse_float(resp)
            self.log(f"  → Temperature: {val:.4f} °C" if val is not None else "  → Temperature: N/A")

    def get_humidity(self):
        resp = self.safe_query_log("SENSor:HUMidity?")
        if resp:
            val = self._parse_float(resp)
            self.log(f"  → Humidity: {val:.2f} %RH" if val is not None else "  → Humidity: N/A (TMP117)")

    def get_sensor_id(self):
        resp = self.safe_query_log("SENSor:ID?")
        if resp:
            try:
                self.log(f"  → Sensor ID: 0x{int(resp):08X}")
            except ValueError:
                pass

    def run_heater(self):
        self.safe_write_log("SENSor:HEATer")

    def soft_reset(self):
        self.safe_write_log("SENSor:SOFTReset")

    # ------------------------------------------------------------------ #
    # Sensor — shared configuration (READperiod / AVErage / PRECision)
    # ------------------------------------------------------------------ #

    def set_read_period(self):
        try:
            val = int(self.period_entry.get())
            if not (50 <= val <= 60000):
                raise ValueError("Period must be 50–60000 ms")
            self.safe_write_log(f"SENSor:READperiod {val}")
        except ValueError as e:
            messagebox.showerror("Error", str(e))

    def get_read_period(self):
        resp = self.safe_query_log("SENSor:READperiod?")
        if resp:
            try:
                val = int(resp)
                self.period_entry.delete(0, tk.END)
                self.period_entry.insert(0, str(val))
                self.log(f"  → Period: {val} ms")
            except ValueError:
                pass

    def set_average(self):
        try:
            val = int(self.average_entry.get())
            # Client-side hint: TMP117 accepts only 1/8/32/64; firmware rejects others.
            if val < 1 or val > 255:
                raise ValueError("Average must be 1–255 (TMP117: only 1, 8, 32, 64)")
            self.safe_write_log(f"SENSor:AVErage {val}")
        except ValueError as e:
            messagebox.showerror("Error", str(e))

    def get_average(self):
        resp = self.safe_query_log("SENSor:AVErage?")
        if resp:
            try:
                val = int(resp)
                self.average_entry.delete(0, tk.END)
                self.average_entry.insert(0, str(val))
                self.log(f"  → Average: {val}")
            except ValueError:
                pass

    def set_precision(self):
        self.safe_write_log(f"SENSor:PRECision {self.precision_var.get()}")

    def get_precision(self):
        resp = self.safe_query_log("SENSor:PRECision?")
        if resp:
            cleaned = resp.strip('"')
            val = self._parse_float(cleaned)
            if val is None and cleaned not in ("nan", "9.91E+37"):
                self.precision_var.set(cleaned)
                self.log(f"  → Precision: {cleaned}")
            else:
                self.log("  → Precision: N/A (TMP117)")

    # ------------------------------------------------------------------ #
    # TMP117 — alert thresholds
    # ------------------------------------------------------------------ #

    def set_alert_high(self):
        try:
            val = float(self.alert_high_entry.get())
            if not (-55.0 <= val <= 150.0):
                raise ValueError("Alert High must be –55 to +150 °C")
            self.safe_write_log(f"SENSor:ALERt:HIGH {val:.4f}")
        except ValueError as e:
            messagebox.showerror("Error", str(e))

    def get_alert_high(self):
        resp = self.safe_query_log("SENSor:ALERt:HIGH?")
        if resp:
            val = self._parse_float(resp)
            if val is not None:
                self.alert_high_entry.delete(0, tk.END)
                self.alert_high_entry.insert(0, f"{val:.4f}")
                self.log(f"  → Alert High: {val:.4f} °C")

    def set_alert_low(self):
        try:
            val = float(self.alert_low_entry.get())
            if not (-55.0 <= val <= 150.0):
                raise ValueError("Alert Low must be –55 to +150 °C")
            self.safe_write_log(f"SENSor:ALERt:LOW {val:.4f}")
        except ValueError as e:
            messagebox.showerror("Error", str(e))

    def get_alert_low(self):
        resp = self.safe_query_log("SENSor:ALERt:LOW?")
        if resp:
            val = self._parse_float(resp)
            if val is not None:
                self.alert_low_entry.delete(0, tk.END)
                self.alert_low_entry.insert(0, f"{val:.4f}")
                self.log(f"  → Alert Low: {val:.4f} °C")

    def get_alert_status(self):
        resp = self.safe_query_log("SENSor:ALERt:STATus?")
        if resp:
            self.log(f"  → Alert Status: {resp.strip(chr(34))}")

    # ------------------------------------------------------------------ #
    # TMP117 — conversion mode and rate
    # ------------------------------------------------------------------ #

    def set_mode(self):
        self.safe_write_log(f"SENSor:MODe {self.mode_var.get()}")

    def get_mode(self):
        resp = self.safe_query_log("SENSor:MODe?")
        if resp:
            cleaned = resp.strip('"')
            self.mode_var.set(cleaned)
            self.log(f"  → Mode: {cleaned}")

    def set_conv_rate(self):
        try:
            val = int(self.conv_rate_var.get())
            if not (0 <= val <= 7):
                raise ValueError("Conv Rate must be 0–7")
            self.safe_write_log(f"SENSor:CONVrate {val}")
        except (ValueError, tk.TclError) as e:
            messagebox.showerror("Error", str(e))

    def get_conv_rate(self):
        resp = self.safe_query_log("SENSor:CONVrate?")
        if resp:
            try:
                val = int(resp)
                self.conv_rate_var.set(val)
                self.log(f"  → Conv Rate: {val} ({CONV_RATE_LABELS.get(val, '?')})")
            except ValueError:
                pass

    # ------------------------------------------------------------------ #
    # Display
    # ------------------------------------------------------------------ #

    def set_brightness(self):
        try:
            val = int(self.brightness_entry.get())
            if not (0 <= val <= 100):
                raise ValueError("Brightness must be 0–100")
            self.safe_write_log(f"DISPlay:BRIGhtness {val}")
        except ValueError as e:
            messagebox.showerror("Error", str(e))

    def get_brightness(self):
        resp = self.safe_query_log("DISPlay:BRIGhtness?")
        if resp:
            try:
                val = int(resp)
                self.brightness_entry.delete(0, tk.END)
                self.brightness_entry.insert(0, str(val))
                self.log(f"  → Brightness: {val}")
            except ValueError:
                pass

    def set_display_state(self):
        self.safe_write_log(f"DISPlay:STATe {self.display_state.get()}")

    def get_display_state(self):
        resp = self.safe_query_log("DISPlay:STATe?")
        if resp:
            self.display_state.set(resp)
            self.log(f"  → Display: {'ON' if resp == '1' else 'OFF'}")

    def set_display_source(self):
        self.safe_write_log(f"DISPlay:SOURce {self.display_source.get()}")

    def get_display_source(self):
        resp = self.safe_query_log("DISPlay:SOURce?")
        if resp:
            self.display_source.set(resp)
            self.log(f"  → Source: {'Measurement' if resp == '0' else 'Text'}")

    def write_text(self):
        text = self.text_entry.get()
        if len(text) > 8:
            messagebox.showerror("Error", "Text must be ≤ 8 characters")
            return
        self.safe_write_log(f"DISPlay:TEXT '{text}'")

    def read_text(self):
        resp = self.safe_query_log("DISPlay:TEXT?")
        if resp:
            self.text_entry.delete(0, tk.END)
            self.text_entry.insert(0, resp)
            self.log(f"  → Text: {resp}")

    # ------------------------------------------------------------------ #
    # Cyclic polling
    # ------------------------------------------------------------------ #

    def start_polling(self):
        try:
            interval = float(self.interval_entry.get())
            if interval <= 0:
                raise ValueError("Interval must be positive")
        except ValueError as e:
            messagebox.showerror("Error", str(e))
            return

        if not self.device:
            messagebox.showerror("Error", "Not connected to device")
            return

        self.start_time = time.time()
        self.timestamps.clear()
        self.values.clear()
        self.open_plot_window()

        self.polling = True
        self.log(f"Started polling ({self.measure_type.get()}) every {interval} s")
        self.poll(interval)

    def stop_polling(self):
        self.polling = False
        self.log("Polling stopped")

    def poll(self, interval):
        if not self.polling:
            return

        if self.measure_type.get() == "TEMP":
            resp = self.safe_query_log("SENSor:TEMPerature?")
            mtype = "TEMP"
        else:
            resp = self.safe_query_log("SENSor:HUMidity?")
            mtype = "HUM"

        if resp is not None:
            val = self._parse_float(resp)
            if val is not None:
                timestamp = time.time() - self.start_time
                self.timestamps.append(timestamp)
                self.values.append(val)
                self.csv_writer.writerow([timestamp, mtype, val])
                self.csv_file.flush()
                self.update_plot()
            else:
                self.log(f"  → Skipping NaN/invalid value: {resp}")

        self.root.after(int(interval * 1000), lambda: self.poll(interval))

    # ------------------------------------------------------------------ #
    # Cleanup
    # ------------------------------------------------------------------ #

    def on_close(self):
        self.polling = False
        try:
            self.csv_file.close()
        except Exception:
            pass
        try:
            if self.device:
                self.device.close()
        except Exception:
            pass
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = SCPIGui(root)
    root.mainloop()
