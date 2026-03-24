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
        self.root.title("SCPI Device GUI")

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

        self.rm = pyvisa.ResourceManager()
        self.device = None

        self.polling = False

        # --- data ---
        self.timestamps = []
        self.values = []
        self.start_time = None  # 🆕 czas startu

        # --- CSV ---
        self.csv_file = open("measurements.csv", "w", newline="")
        self.csv_writer = csv.writer(self.csv_file)
        self.csv_writer.writerow(["timestamp", "type", "value"])

        self.create_widgets()
        self.refresh_resources()

        self.setup_plot()

    def create_widgets(self):
        conn_frame = ttk.LabelFrame(self.root, text="Connection")
        conn_frame.grid(row=0, column=0, padx=10, pady=5, sticky="ew")

        self.resource_combo = ttk.Combobox(conn_frame, width=40)
        self.resource_combo.grid(row=0, column=0, padx=5, pady=5)

        ttk.Button(conn_frame, text="Refresh", command=self.refresh_resources).grid(row=0, column=1)
        ttk.Button(conn_frame, text="Connect", command=self.connect).grid(row=0, column=2)
        ttk.Button(conn_frame, text="Disonnect", command=self.disconnect).grid(row=0, column=3)

        ttk.Button(self.root, text="*IDN?", command=self.get_idn, width=60).grid(row=1, column=0, pady=5)

        sensor_frame = ttk.LabelFrame(self.root, text="Sensor")
        sensor_frame.grid(row=2, column=0, padx=10, pady=5, sticky="ew")

        ttk.Button(sensor_frame, text="Type?", command=self.get_sensor_type).grid(row=0, column=0)
        ttk.Button(sensor_frame, text="Temperature?", command=self.get_temperature).grid(row=0, column=1)
        ttk.Button(sensor_frame, text="Humidity?", command=self.get_humidity).grid(row=0, column=2)
        ttk.Button(sensor_frame, text="ID?", command=self.get_sensor_id).grid(row=0, column=3)
        ttk.Button(sensor_frame, text="Heater", command=self.run_heater).grid(row=0, column=4)

        cycle_frame = ttk.LabelFrame(self.root, text="Cyclic Measurement")
        cycle_frame.grid(row=3, column=0, padx=10, pady=5, sticky="ew")

        self.measure_type = tk.StringVar(value="TEMP")

        ttk.Radiobutton(cycle_frame, text="Temperature", variable=self.measure_type, value="TEMP").grid(row=0, column=0)
        ttk.Radiobutton(cycle_frame, text="Humidity", variable=self.measure_type, value="HUM").grid(row=0, column=1)

        ttk.Label(cycle_frame, text="Interval [s]:").grid(row=1, column=0)
        self.interval_entry = ttk.Entry(cycle_frame, width=10)
        self.interval_entry.insert(0, "2")
        self.interval_entry.grid(row=1, column=1)

        ttk.Button(cycle_frame, text="Start", command=self.start_polling).grid(row=2, column=0)
        ttk.Button(cycle_frame, text="Stop", command=self.stop_polling).grid(row=2, column=1)
        ttk.Button(cycle_frame, text="Open Plot", command=self.open_plot_window).grid(row=2, column=2)
        ttk.Button(cycle_frame, text="Clear Plot", command=self.clear_plot_window).grid(row=2, column=3)

        display_frame = ttk.LabelFrame(self.root, text="Display")
        display_frame.grid(row=4, column=0, padx=10, pady=5, sticky="ew")

        ttk.Label(display_frame, text="Brightness (0-100):").grid(row=0, column=0)
        self.brightness_entry = ttk.Entry(display_frame, width=10)
        self.brightness_entry.grid(row=0, column=1)
        self.brightness_entry.insert(0, "5")
        ttk.Button(display_frame, text="Set", command=self.set_brightness).grid(row=0, column=2)
        ttk.Button(display_frame, text="Get", command=self.get_brightness).grid(row=0, column=3)

        self.display_state = tk.StringVar(value="1")
        ttk.Radiobutton(display_frame, text="ON", variable=self.display_state, value="1").grid(row=1, column=0)
        ttk.Radiobutton(display_frame, text="OFF", variable=self.display_state, value="0").grid(row=1, column=1)
        ttk.Button(display_frame, text="Set State", command=self.set_display_state).grid(row=1, column=2)
        ttk.Button(display_frame, text="Get State", command=self.get_display_state).grid(row=1, column=3)
        
        self.display_source = tk.StringVar(value="0")
        ttk.Radiobutton(display_frame, text="Meas", variable=self.display_source, value="0").grid(row=2, column=0)
        ttk.Radiobutton(display_frame, text="Text", variable=self.display_source, value="1").grid(row=2, column=1)
        ttk.Button(display_frame, text="Set Source", command=self.set_display_source).grid(row=2, column=2)
        ttk.Button(display_frame, text="Get Source", command=self.get_display_source).grid(row=2, column=3)
        
        ttk.Label(display_frame, text="Text:").grid(row=3, column=0)
        self.text_entry = ttk.Entry(display_frame, width=10)
        self.text_entry.grid(row=3, column=1)
        self.text_entry.insert(0, "????????")
        ttk.Button(display_frame, text="Write", command=self.write_text).grid(row=3, column=2)
        ttk.Button(display_frame, text="Read", command=self.read_text).grid(row=3, column=3)

        system_frame = ttk.LabelFrame(self.root, text="System")
        system_frame.grid(row=5, column=0, padx=10, pady=5, sticky="ew")

        ttk.Button(system_frame, text="Enter Bootloader", command=self.enter_bootloader).grid(row=0, column=0)
        ttk.Button(system_frame, text="Restart", command=self.restart).grid(row=0, column=1)

        self.output = tk.Text(self.root, height=10, width=60)
        self.output.grid(row=6, column=0, padx=10, pady=10)

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
        self.plot_window.title("Live Plot")

        def on_close():
            self.plot_window.destroy()
            self.plot_window = None

        self.plot_window.protocol("WM_DELETE_WINDOW", on_close)

        self.fig, self.ax = plt.subplots(figsize=(6, 4))
        self.line, = self.ax.plot([], [], '-o')

        self.ax.set_title("Live Measurement")
        self.ax.set_xlabel("Time [s]")
        self.ax.set_ylabel("Value")

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.plot_window)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
    def clear_plot_window(self):
        self.start_time = time.time()
        self.timestamps.clear()
        self.values.clear()
        self.update_plot()

    def update_plot(self):
        if self.plot_window is None:
            return

        self.line.set_data(self.timestamps, self.values)
        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas.draw_idle()

    # --- LOG ---
    def log(self, text):
        self.output.insert(tk.END, text + "\n")
        self.output.see(tk.END)

    # --- CONNECTION ---
    def refresh_resources(self):
        try:
            resources = self.rm.list_resources()
            self.resource_combo['values'] = resources
            if resources:
                self.resource_combo.current(0)
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def connect(self):
        try:
            resource = self.resource_combo.get()
            self.device = self.rm.open_resource(resource)
            self.log(f"Connected to {resource}")
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))
    
    def disconnect(self):
        if not self.device:
            raise Exception("Not connected")
        try:
            device = self.device.resource_name
            self.device.close()
            self.device = None
            self.log(f"Disonnected from {device}")
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))

    def query(self, cmd):
        if not self.device:
            raise Exception("Not connected")
        return self.device.query(cmd).strip()

    def safe_query_log(self, cmd):
        try:
            resp = self.query(cmd)
            self.log(f"{cmd} -> {resp}")
            return resp
        except Exception as e:
            self.log(f"ERROR: {e}")
            self.stop_polling()

    def write(self, cmd):
        try:
            if not self.device:
                raise Exception("Not connected")
            self.device.write(cmd)
            self.log(f"{cmd} sent")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    # --- SCPI ---
    def get_idn(self):
        self.safe_query_log("*IDN?")

    def get_sensor_type(self):
        self.safe_query_log("SENSor:TYPE?")

    def get_temperature(self):
        self.safe_query_log("SENSor:TEMPerature?")

    def get_humidity(self):
        self.safe_query_log("SENSor:HUMidity?")

    def get_sensor_id(self):
        try:
            resp = self.query("SENSor:ID?")
            val = int(resp)
            self.log(f"SENSor:ID? -> 0x{val:08X}")
        except Exception as e:
            messagebox.showerror("Error", str(e))
            
    def run_heater(self):
        self.safe_query_log("SENSor:HEATer")


    def set_brightness(self):
        try:
            val = int(self.brightness_entry.get())
            if not (0 <= val <= 100):
                raise ValueError
        except:
            messagebox.showerror("Error", "Brightness must be 0-100")
            return

        self.write(f"DISPlay:BRIGhtness {val}")
    
    def get_brightness(self):
        state = ' '
        state = self.safe_query_log("DISPlay:BRIGhtness?")
        self.brightness_entry.delete(0,100)
        self.brightness_entry.insert(0,state)

    def set_display_state(self):
        state = self.display_state.get()
        self.write(f"DISPlay:STATe {state}")
    
    def get_display_state(self):
        state = self.safe_query_log("DISPlay:STATe?")
        self.display_state.set(state)
        
    def set_display_source(self):
        source = self.display_source.get()
        self.write(f"DISPlay:SOURce {source}")
            
    def get_display_source(self):
        source = self.safe_query_log("DISPlay:SOURce?")
        self.display_source.set(source)
    
    def write_text(self):
        text = self.text_entry.get()
        if len(text) <= 8:
            self.write(f"DISPlay:TEXT '{text}'")
        else:
            messagebox.showerror("Error"," Wprowadzony tekst za długi!")
            
    def read_text(self):
        text = self.safe_query_log("DISPlay:TEXT?")
        self.text_entry.delete(0,100)
        self.text_entry.insert(0,text)
            
    def enter_bootloader(self):
        if messagebox.askyesno("Confirm", "Device will restart into bootloader"):
            self.write("SYSTem:BOOTloader:ENter")
        self.device = None
            
    def restart(self):
        if messagebox.askyesno("Confirm", "Device will restart"):
            self.write("SYSTem:RST")
        self.device = None
            

    # --- POLLING ---
    def start_polling(self):
        try:
            interval = float(self.interval_entry.get())
            if interval <= 0:
                raise ValueError
        except:
            messagebox.showerror("Error", "Invalid interval")
            return

        # 🆕 reset czasu i danych
        self.start_time = time.time()
        self.timestamps.clear()
        self.values.clear()

        # 🆕 auto open plot
        self.open_plot_window()

        self.polling = True
        self.log("Started polling")
        self.poll(interval)

    def stop_polling(self):
        self.polling = False
        self.log("Stopped polling")

    def poll(self, interval):
        if not self.polling:
            return

        if self.measure_type.get() == "TEMP":
            resp = self.safe_query_log("SENSor:TEMPerature?")
            mtype = "TEMP"
        else:
            resp = self.safe_query_log("SENSor:HUMidity?")
            mtype = "HUM"

        try:
            value = float(resp)

            # 🆕 czas od startu
            timestamp = time.time() - self.start_time

            self.timestamps.append(timestamp)
            self.values.append(value)

            self.csv_writer.writerow([timestamp, mtype, value])
            self.csv_file.flush()

            self.update_plot()

        except:
            pass

        self.root.after(int(interval * 1000), lambda: self.poll(interval))

    def on_close(self):
        self.polling = False

        try:
            self.csv_file.close()
        except:
            pass

        try:
            if self.device:
                self.device.close()
        except:
            pass
        
        self.root.destroy()
        quit()

if __name__ == "__main__":
    root = tk.Tk()
    app = SCPIGui(root)
    root.mainloop()