import tkinter as tk
from tkinter import ttk, messagebox
import pyvisa
import csv
import time

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg


class SCPIGui:
    def __init__(self, root):
        self.root = root
        self.root.title("SCPI Device GUI - SDT Board")
        #self.root.geometry("700x900")

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

        self.rm = pyvisa.ResourceManager()
        self.device = None

        self.polling = False

        # --- data ---
        self.timestamps = []
        self.values = []
        self.start_time = None

        # --- CSV ---
        self.csv_file = open("measurements.csv", "w", newline="")
        self.csv_writer = csv.writer(self.csv_file)
        self.csv_writer.writerow(["timestamp", "type", "value"])

        self.create_widgets()
        self.refresh_resources()
        self.setup_plot()

    def create_widgets(self):
        # --- Connection Frame ---
        conn_frame = ttk.LabelFrame(self.root, text="Connection")
        conn_frame.grid(row=0, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        self.resource_combo = ttk.Combobox(conn_frame, width=40)
        self.resource_combo.grid(row=0, column=0, padx=5, pady=5)

        ttk.Button(conn_frame, text="Refresh", command=self.refresh_resources).grid(row=0, column=1, padx=2)
        ttk.Button(conn_frame, text="Connect", command=self.connect).grid(row=0, column=2, padx=2)
        ttk.Button(conn_frame, text="Disconnect", command=self.disconnect).grid(row=0, column=3, padx=2)

        ttk.Button(self.root, text="*IDN?", command=self.get_idn, width=60).grid(row=1, column=0, pady=5)

        # --- Sensor Frame ---
        sensor_frame = ttk.LabelFrame(self.root, text="Sensor")
        sensor_frame.grid(row=2, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        ttk.Button(sensor_frame, text="Type?", command=self.get_sensor_type).grid(row=0, column=0, padx=2)
        ttk.Button(sensor_frame, text="Temperature?", command=self.get_temperature).grid(row=0, column=1, padx=2)
        ttk.Button(sensor_frame, text="Humidity?", command=self.get_humidity).grid(row=0, column=2, padx=2)
        ttk.Button(sensor_frame, text="ID?", command=self.get_sensor_id).grid(row=0, column=3, padx=2)
        ttk.Button(sensor_frame, text="Heater ON", command=self.run_heater).grid(row=0, column=4, padx=2)

        # --- SHT45 Configuration Frame (NEW) ---
        config_frame = ttk.LabelFrame(self.root, text="SHT45 Configuration")
        config_frame.grid(row=3, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        # Read Period
        ttk.Label(config_frame, text="Read Period [ms]:").grid(row=0, column=0, sticky="w")
        self.period_entry = ttk.Entry(config_frame, width=12)
        self.period_entry.insert(0, "500")
        self.period_entry.grid(row=0, column=1, padx=5)
        ttk.Button(config_frame, text="Set", command=self.set_read_period).grid(row=0, column=2, padx=2)
        ttk.Button(config_frame, text="Get", command=self.get_read_period).grid(row=0, column=3, padx=2)

        # Measurement Average
        ttk.Label(config_frame, text="Average Count (1-255):").grid(row=1, column=0, sticky="w")
        self.average_entry = ttk.Entry(config_frame, width=12)
        self.average_entry.insert(0, "1")
        self.average_entry.grid(row=1, column=1, padx=5)
        ttk.Button(config_frame, text="Set", command=self.set_average).grid(row=1, column=2, padx=2)
        ttk.Button(config_frame, text="Get", command=self.get_average).grid(row=1, column=3, padx=2)


        # Precision
        ttk.Label(config_frame, text="Precision:").grid(row=2, column=0, sticky="w")
        self.precision_var = tk.StringVar(value="HIGH")
        precision_combo = ttk.Combobox(config_frame, textvariable=self.precision_var, 
                                       values=["LOW", "MEDIUM", "HIGH"], state="readonly", width=10)
        precision_combo.grid(row=2, column=1, padx=5)
        ttk.Button(config_frame, text="Set", command=self.set_precision).grid(row=2, column=2, padx=2)
        ttk.Button(config_frame, text="Get", command=self.get_precision).grid(row=2, column=3, padx=2)

        # --- Cyclic Measurement Frame ---
        cycle_frame = ttk.LabelFrame(self.root, text="Cyclic Measurement")
        cycle_frame.grid(row=4, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        self.measure_type = tk.StringVar(value="TEMP")

        ttk.Radiobutton(cycle_frame, text="Temperature", variable=self.measure_type, value="TEMP").grid(row=0, column=0)
        ttk.Radiobutton(cycle_frame, text="Humidity", variable=self.measure_type, value="HUM").grid(row=0, column=1)

        ttk.Label(cycle_frame, text="Interval [s]:").grid(row=1, column=0)
        self.interval_entry = ttk.Entry(cycle_frame, width=10)
        self.interval_entry.insert(0, "2")
        self.interval_entry.grid(row=1, column=1)

        ttk.Button(cycle_frame, text="Start", command=self.start_polling).grid(row=2, column=0, padx=2)
        ttk.Button(cycle_frame, text="Stop", command=self.stop_polling).grid(row=2, column=1, padx=2)
        ttk.Button(cycle_frame, text="Open Plot", command=self.open_plot_window).grid(row=2, column=2, padx=2)
        ttk.Button(cycle_frame, text="Clear Plot", command=self.clear_plot_window).grid(row=2, column=3, padx=2)

        # --- Display Frame ---
        display_frame = ttk.LabelFrame(self.root, text="Display")
        display_frame.grid(row=5, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        ttk.Label(display_frame, text="Brightness (0-100):").grid(row=0, column=0)
        self.brightness_entry = ttk.Entry(display_frame, width=10)
        self.brightness_entry.insert(0, "5")
        self.brightness_entry.grid(row=0, column=1)
        ttk.Button(display_frame, text="Set", command=self.set_brightness).grid(row=0, column=2, padx=2)
        ttk.Button(display_frame, text="Get", command=self.get_brightness).grid(row=0, column=3, padx=2)

        self.display_state = tk.StringVar(value="1")
        ttk.Radiobutton(display_frame, text="ON", variable=self.display_state, value="1").grid(row=1, column=0)
        ttk.Radiobutton(display_frame, text="OFF", variable=self.display_state, value="0").grid(row=1, column=1)
        ttk.Button(display_frame, text="Set State", command=self.set_display_state).grid(row=1, column=2, padx=2)
        ttk.Button(display_frame, text="Get State", command=self.get_display_state).grid(row=1, column=3, padx=2)
        
        self.display_source = tk.StringVar(value="0")
        ttk.Radiobutton(display_frame, text="Meas", variable=self.display_source, value="0").grid(row=2, column=0)
        ttk.Radiobutton(display_frame, text="Text", variable=self.display_source, value="1").grid(row=2, column=1)
        ttk.Button(display_frame, text="Set Source", command=self.set_display_source).grid(row=2, column=2, padx=2)
        ttk.Button(display_frame, text="Get Source", command=self.get_display_source).grid(row=2, column=3, padx=2)
        
        ttk.Label(display_frame, text="Text (max 8 char):").grid(row=3, column=0)
        self.text_entry = ttk.Entry(display_frame, width=10)
        self.text_entry.insert(0, "????????")
        self.text_entry.grid(row=3, column=1)
        ttk.Button(display_frame, text="Write", command=self.write_text).grid(row=3, column=2, padx=2)
        ttk.Button(display_frame, text="Read", command=self.read_text).grid(row=3, column=3, padx=2)

        # --- System Frame ---
        system_frame = ttk.LabelFrame(self.root, text="System")
        system_frame.grid(row=6, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        ttk.Button(system_frame, text="Enter Bootloader", command=self.enter_bootloader).grid(row=0, column=0, padx=2)
        ttk.Button(system_frame, text="Restart", command=self.restart).grid(row=0, column=1, padx=2)
        
        self.id_type = tk.StringVar(value="SHORT")
        ttk.Radiobutton(system_frame, text="Short", variable=self.id_type, value="SHORT").grid(row=1, column=0)
        ttk.Radiobutton(system_frame, text="Long", variable=self.id_type, value="LONG").grid(row=1, column=1)
        ttk.Button(system_frame, text="Get ID", command=self.get_device_id).grid(row=1, column=2, padx=2)

        # --- Output Log ---
        self.output = tk.Text(self.root, height=8, width=60)
        self.output.grid(row=7, column=0, padx=10, pady=10, sticky="nsew")
        
        # Add scrollbar
        scrollbar = ttk.Scrollbar(self.root, orient="vertical", command=self.output.yview)
        scrollbar.grid(row=7, column=1, sticky="ns")
        self.output.config(yscrollcommand=scrollbar.set)

    # --- PLOT ---
    def setup_plot(self):
        self.plot_window = None
        self.fig = None
        self.ax = None
        self.line = None
        self.canvas = None

    def open_plot_window(self):
        if self.plot_window is not None:
            return

        self.plot_window = tk.Toplevel(self.root)
        self.plot_window.title("Live Measurement Plot")

        def on_close():
            self.plot_window.destroy()
            self.plot_window = None

        self.plot_window.protocol("WM_DELETE_WINDOW", on_close)

        self.fig, self.ax = plt.subplots(figsize=(8, 5))
        self.line, = self.ax.plot([], [], '-o', linewidth=2, markersize=4)

        self.ax.set_title("Live Measurement", fontsize=12, fontweight='bold')
        self.ax.set_xlabel("Time [s]", fontsize=10)
        self.ax.set_ylabel("Value", fontsize=10)
        self.ax.grid(True, alpha=0.3)

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.plot_window)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
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

    # --- LOGGING ---
    def log(self, text):
        """Log message to output text widget"""
        self.output.insert(tk.END, f"[{time.strftime('%H:%M:%S')}] {text}\n")
        self.output.see(tk.END)

    # --- CONNECTION ---
    def refresh_resources(self):
        """Refresh list of available VISA resources"""
        try:
            resources = self.rm.list_resources()
            self.resource_combo['values'] = resources
            if resources:
                self.resource_combo.current(0)
                self.log(f"Found {len(resources)} device(s)")
        except Exception as e:
            self.log(f"ERROR: Failed to refresh resources: {e}")

    def connect(self):
        """Connect to selected VISA resource"""
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
        """Disconnect from device"""
        if not self.device:
            messagebox.showerror("Error", "Not connected")
            return
        try:
            device_name = self.device.resource_name
            self.device.close()
            self.device = None
            self.log(f"✓ Disconnected from {device_name}")
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))
            self.log(f"✗ Disconnect failed: {e}")

    def query(self, cmd):
        """Send SCPI command and get response"""
        if not self.device:
            raise Exception("Not connected to device")
        return self.device.query(cmd).strip()

    def write(self, cmd):
        """Send SCPI command without response"""
        if not self.device:
            raise Exception("Not connected to device")
        self.device.write(cmd)

    def safe_query_log(self, cmd):
        """Execute query with error logging"""
        try:
            resp = self.query(cmd)
            self.log(f"✓ {cmd} → {resp}")
            return resp
        except Exception as e:
            self.log(f"✗ ERROR [{cmd}]: {e}")
            self.stop_polling()
            return None

    def safe_write_log(self, cmd):
        """Execute write with error logging"""
        try:
            self.write(cmd)
            self.log(f"✓ {cmd}")
        except Exception as e:
            self.log(f"✗ ERROR [{cmd}]: {e}")
            messagebox.showerror("Error", str(e))

    # --- SCPI COMMANDS - System ---
    def get_idn(self):
        """Get device identification"""
        self.safe_query_log("*IDN?")

    def get_device_id(self):
        """Get device STM32 ID"""
        dev_id_type = self.id_type.get()
        self.safe_query_log(f"SYSTem:ID? {dev_id_type}")

    def enter_bootloader(self):
        """Restart device into bootloader"""
        if messagebox.askyesno("Confirm", "Device will restart into bootloader mode. Continue?"):
            self.safe_write_log("SYSTem:BOOTloader:ENter")
            self.device = None

    def restart(self):
        """Restart device"""
        if messagebox.askyesno("Confirm", "Device will restart. Continue?"):
            self.safe_write_log("SYSTem:RST")
            self.device = None

    # --- SCPI COMMANDS - Sensor ---
    def get_sensor_type(self):
        """Get sensor type"""
        self.safe_query_log("SENSor:TYPE?")

    def get_temperature(self):
        """Get temperature measurement"""
        resp = self.safe_query_log("SENSor:TEMPerature?")
        if resp:
            try:
                val = float(resp)
                self.log(f"  → Temperature: {val:.2f}°C")
            except ValueError:
                self.log(f"  → Invalid temperature value: {resp}")

    def get_humidity(self):
        """Get humidity measurement"""
        resp = self.safe_query_log("SENSor:HUMidity?")
        if resp:
            try:
                val = float(resp)
                self.log(f"  → Humidity: {val:.2f}%RH")
            except ValueError:
                self.log(f"  → Invalid humidity value: {resp}")

    def get_sensor_id(self):
        """Get sensor serial number"""
        resp = self.safe_query_log("SENSor:ID?")
        if resp:
            try:
                val = int(resp)
                self.log(f"  → Sensor ID: 0x{val:08X}")
            except ValueError:
                pass

    def run_heater(self):
        """Activate SHT45 built-in heater"""
        self.safe_write_log("SENSor:HEATer")

    # --- SCPI COMMANDS - SHT45 Configuration (NEW) ---
    def set_read_period(self):
        """Set measurement read period [ms]"""
        try:
            val = int(self.period_entry.get())
            if not (50 <= val <= 60000):
                raise ValueError("Period must be 50-60000 ms")
            self.safe_write_log(f"SENSor:READperiod {val}")
        except ValueError as e:
            messagebox.showerror("Error", str(e))

    def get_read_period(self):
        """Get measurement read period [ms]"""
        resp = self.safe_query_log("SENSor:READperiod?")
        if resp:
            try:
                val = int(resp)
                self.period_entry.delete(0, tk.END)
                self.period_entry.insert(0, str(val))
                self.log(f"  → Period: {val} ms")
            except ValueError:
                pass

    def set_nplc(self):
        """Set measurement average count (1-255)"""
        try:
            val = int(self.average_entry.get())
            if not (1 <= val <= 255):
                raise ValueError("Average count must be 1-255")
            self.safe_write_log(f"SENSor:AVErage {val}")
        except ValueError as e:
            messagebox.showerror("Error", str(e))

    def get_nplc(self):
        """Get measurement average count"""
        resp = self.safe_query_log("SENSor:AVErage?")
        if resp:
            try:
                val = int(resp)
                self.average_entry.delete(0, tk.END)
                self.average_entry.insert(0, str(val))
                self.log(f"  → Average: {val}")
            except ValueError:
                pass

    # Aliases for backward compatibility
    def set_average(self):
        """Alias for set_nplc - Set measurement average count"""
        return self.set_nplc()

    def get_average(self):
        """Alias for get_nplc - Get measurement average count"""
        return self.get_nplc()

    def set_precision(self):
        """Set measurement precision (LOW/MEDIUM/HIGH)"""
        precision = self.precision_var.get()
        self.safe_write_log(f"SENSor:PRECision {precision}")

    def get_precision(self):
        """Get measurement precision"""
        resp = self.safe_query_log("SENSor:PRECision?").replace('"', "")
        if resp:
            self.precision_var.set(resp)
            self.log(f"  → Precision: {resp}")

    # --- SCPI COMMANDS - Display ---
    def set_brightness(self):
        """Set display brightness (0-100)"""
        try:
            val = int(self.brightness_entry.get())
            if not (0 <= val <= 100):
                raise ValueError("Brightness must be 0-100")
            self.safe_write_log(f"DISPlay:BRIGhtness {val}")
        except ValueError as e:
            messagebox.showerror("Error", str(e))
    
    def get_brightness(self):
        """Get display brightness"""
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
        """Set display ON/OFF"""
        state = self.display_state.get()
        self.safe_write_log(f"DISPlay:STATe {state}")
    
    def get_display_state(self):
        """Get display state"""
        resp = self.safe_query_log("DISPlay:STATe?")
        if resp:
            self.display_state.set(resp)
            self.log(f"  → Display: {'ON' if resp == '1' else 'OFF'}")

    def set_display_source(self):
        """Set display source (0=Measurement, 1=Text)"""
        source = self.display_source.get()
        self.safe_write_log(f"DISPlay:SOURce {source}")
            
    def get_display_source(self):
        """Get display source"""
        resp = self.safe_query_log("DISPlay:SOURce?")
        if resp:
            self.display_source.set(resp)
            self.log(f"  → Source: {'Measurement' if resp == '0' else 'Text'}")
    
    def write_text(self):
        """Write text to display (max 8 characters)"""
        text = self.text_entry.get()
        if len(text) > 8:
            messagebox.showerror("Error", "Text is too long (max 8 characters)")
            return
        self.safe_write_log(f"DISPlay:TEXT '{text}'")
            
    def read_text(self):
        """Read text from display"""
        resp = self.safe_query_log("DISPlay:TEXT?")
        if resp:
            self.text_entry.delete(0, tk.END)
            self.text_entry.insert(0, resp)
            self.log(f"  → Text: {resp}")
            

    # --- CYCLIC POLLING ---
    def start_polling(self):
        """Start cyclic measurement"""
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

        # Reset time and data
        self.start_time = time.time()
        self.timestamps.clear()
        self.values.clear()

        # Auto-open plot
        self.open_plot_window()

        self.polling = True
        self.log(f"Started polling ({self.measure_type.get()}) every {interval}s")
        self.poll(interval)

    def stop_polling(self):
        """Stop cyclic measurement"""
        self.polling = False
        self.log("Polling stopped")

    def poll(self, interval):
        """Periodic measurement task"""
        if not self.polling:
            return

        # Select measurement type
        if self.measure_type.get() == "TEMP":
            resp = self.safe_query_log("SENSor:TEMPerature?")
            mtype = "TEMP"
        else:
            resp = self.safe_query_log("SENSor:HUMidity?")
            mtype = "HUM"

        if resp is not None:
            try:
                value = float(resp)

                # Calculate time from start
                timestamp = time.time() - self.start_time

                self.timestamps.append(timestamp)
                self.values.append(value)

                self.csv_writer.writerow([timestamp, mtype, value])
                self.csv_file.flush()

                self.update_plot()

            except ValueError:
                self.log(f"ERROR: Invalid value received: {resp}")

        # Schedule next measurement
        self.root.after(int(interval * 1000), lambda: self.poll(interval))

    # --- CLEANUP ---
    def on_close(self):
        """Handle application close"""
        self.polling = False

        try:
            self.csv_file.close()
            self.log("CSV file saved")
        except Exception as e:
            self.log(f"Error closing CSV: {e}")

        try:
            if self.device:
                self.device.close()
                self.log("Device disconnected")
        except Exception as e:
            self.log(f"Error disconnecting: {e}")
        
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = SCPIGui(root)
    root.mainloop()