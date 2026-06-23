#!/usr/bin/env python3
"""
Suzuki SDS Live Dashboard
Reads real-time sensor data from Arduino-based K-Line interface
"""

import serial
import serial.tools.list_ports
import time
import threading
import csv
from datetime import datetime
from collections import deque
import tkinter as tk
from tkinter import ttk, scrolledtext
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

SERIAL_BAUD = 115200
DATA_POINTS = 200

class SDSDashboard:
    def __init__(self, root):
        self.root = root
        self.root.title("Suzuki SDS Dashboard — 2004 GSX-R1000")
        self.root.geometry("1280x800")
        self.root.configure(bg="#1a1a2e")

        self.serial_port = None
        self.running = False
        self.connected = False
        self.dealer_mode = False
        self.bt_mode = False

        self.data = {
            "RPM": deque([0]*DATA_POINTS, maxlen=DATA_POINTS),
            "TPS": deque([0]*DATA_POINTS, maxlen=DATA_POINTS),
            "SPEED": deque([0]*DATA_POINTS, maxlen=DATA_POINTS),
            "ECT": deque([0]*DATA_POINTS, maxlen=DATA_POINTS),
            "IAT": deque([0]*DATA_POINTS, maxlen=DATA_POINTS),
            "VOLT": deque([0]*DATA_POINTS, maxlen=DATA_POINTS),
        }
        self.time_data = deque([0]*DATA_POINTS, maxlen=DATA_POINTS)
        self.sample_count = 0

        self.csv_file = None
        self.csv_writer = None
        self.logging_enabled = False

        self.build_ui()
        self.poll_serial()

    def build_ui(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("Dark.TFrame", background="#1a1a2e")
        style.configure("Dark.TLabel", background="#1a1a2e", foreground="#e0e0e0")
        style.configure("Dark.TButton", background="#16213e", foreground="#e0e0e0")
        style.configure("Value.TLabel", background="#1a1a2e", foreground="#00ff88", font=("Consolas", 14, "bold"))
        style.configure("Title.TLabel", background="#1a1a2e", foreground="#e94560", font=("Arial", 12, "bold"))

        main_frame = ttk.Frame(self.root, style="Dark.TFrame")
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        top_frame = ttk.Frame(main_frame, style="Dark.TFrame")
        top_frame.pack(fill=tk.X)

        ttk.Label(top_frame, text="🔌 Serial Port:", style="Dark.TLabel").pack(side=tk.LEFT, padx=5)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(top_frame, textvariable=self.port_var, width=20)
        self.port_combo.pack(side=tk.LEFT, padx=5)
        self.refresh_ports()

        ttk.Button(top_frame, text="🔄 Refresh", command=self.refresh_ports).pack(side=tk.LEFT, padx=2)
        self.connect_btn = ttk.Button(top_frame, text="🔗 Connect", command=self.toggle_connect)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        ttk.Button(top_frame, text="⚡ Dealer", command=self.toggle_dealer).pack(side=tk.LEFT, padx=2)
        ttk.Button(top_frame, text="📋 DTC", command=self.read_dtc).pack(side=tk.LEFT, padx=2)
        self.log_btn = ttk.Button(top_frame, text="💾 Log", command=self.toggle_logging)
        self.log_btn.pack(side=tk.LEFT, padx=2)

        self.status_label = ttk.Label(top_frame, text="Disconnected", style="Dark.TLabel")
        self.status_label.pack(side=tk.RIGHT, padx=10)

        gauge_frame = ttk.Frame(main_frame, style="Dark.TFrame")
        gauge_frame.pack(fill=tk.BOTH, expand=False, pady=5)

        self.gauge_labels = {}
        gauges = [("RPM", "0"), ("SPEED", "0 km/h"), ("TPS", "0%"),
                  ("ECT", "0°C"), ("IAT", "0°C"), ("VOLT", "0.0V"),
                  ("GEAR", "N"), ("MAP", "0 kPa")]
        for i, (name, unit) in enumerate(gauges):
            f = ttk.Frame(gauge_frame, style="Dark.TFrame", relief=tk.RIDGE, borderwidth=2)
            f.grid(row=i//4, column=i%4, padx=4, pady=4, sticky="nsew")
            ttk.Label(f, text=name, style="Title.TLabel").pack()
            self.gauge_labels[name] = ttk.Label(f, text=unit, style="Value.TLabel")
            self.gauge_labels[name].pack()
        gauge_frame.grid_columnconfigure(tuple(range(4)), weight=1)

        graph_frame = ttk.Frame(main_frame, style="Dark.TFrame")
        graph_frame.pack(fill=tk.BOTH, expand=True)

        self.fig = Figure(figsize=(12, 4), dpi=80, facecolor="#1a1a2e")
        self.fig.subplots_adjust(hspace=0.4)

        self.ax1 = self.fig.add_subplot(121)
        self.ax2 = self.fig.add_subplot(122)

        for ax in (self.ax1, self.ax2):
            ax.set_facecolor("#16213e")
            ax.tick_params(colors="#e0e0e0")
            ax.spines["bottom"].set_color("#333")
            ax.spines["top"].set_color("#333")
            ax.spines["left"].set_color("#333")
            ax.spines["right"].set_color("#333")
            ax.grid(True, alpha=0.3)

        self.rpm_line, = self.ax1.plot([], [], color="#e94560", linewidth=1.5, label="RPM")
        self.ax1.set_title("Engine RPM", color="#e0e0e0")
        self.ax1.set_ylabel("RPM", color="#e0e0e0")
        self.ax1.legend(loc="upper right")

        self.tps_line, = self.ax2.plot([], [], color="#00ff88", linewidth=1.5, label="TPS %")
        self.ect_line, = self.ax2.plot([], [], color="#ffa500", linewidth=1.5, label="Coolant °C")
        self.ax2.set_title("TPS & Coolant", color="#e0e0e0")
        self.ax2.legend(loc="upper right")

        self.canvas = FigureCanvasTkAgg(self.fig, master=graph_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        log_frame = ttk.Frame(main_frame, style="Dark.TFrame")
        log_frame.pack(fill=tk.BOTH, expand=False)

        self.log_text = scrolledtext.ScrolledText(log_frame, height=8, bg="#0f0f23",
                                                    fg="#e0e0e0", font=("Consolas", 9))
        self.log_text.pack(fill=tk.BOTH, expand=True)

    def refresh_ports(self):
        import glob
        ports = [p.device for p in serial.tools.list_ports.comports()]
        # Add Bluetooth SPP ports (Windows: COM*, Linux: /dev/rfcomm*, macOS: /dev/tty.*)
        try:
            bt_ports = glob.glob("/dev/rfcomm*") if os.name != "nt" else []
            ports.extend(bt_ports)
        except:
            pass
        self.port_combo["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def toggle_connect(self):
        if self.connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_var.get()
        if not port:
            self.log("No port selected")
            return
        try:
            if "rfcomm" in port or "COM" in port:
                self.bt_mode = "rfcomm" in port
            self.serial_port = serial.Serial(port, SERIAL_BAUD, timeout=2)
            time.sleep(2)
            self.serial_port.reset_input_buffer()
            self.connected = True
            self.connect_btn.config(text="🔌 Disconnect")
            medium = "BT" if self.bt_mode else "USB"
            self.status_label.config(text=f"{medium}: {port}")
            self.log(f"Connected to {port} ({medium})")
            self.running = True
            threading.Thread(target=self.read_loop, daemon=True).start()
        except Exception as e:
            self.log(f"Connection error: {e}")

    def disconnect(self):
        self.running = False
        if self.serial_port:
            try:
                self.serial_port.close()
            except:
                pass
        self.connected = False
        self.connect_btn.config(text="🔗 Connect")
        self.status_label.config(text="Disconnected")
        self.log("Disconnected")

    def toggle_dealer(self):
        if not self.connected:
            self.log("Not connected")
            return
        self.dealer_mode = not self.dealer_mode
        self.send_cmd("d")
        self.log(f"Dealer mode {'ON' if self.dealer_mode else 'OFF'}")

    def read_dtc(self):
        if not self.connected:
            self.log("Not connected")
            return
        self.send_cmd("t")
        self.log("Reading DTCs...")

    def toggle_logging(self):
        if not self.logging_enabled:
            fname = f"sds_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
            self.csv_file = open(fname, "w", newline="")
            self.csv_writer = csv.writer(self.csv_file)
            self.csv_writer.writerow(["Time", "RPM", "TPS", "SPEED", "ECT", "IAT", "VOLT", "GEAR", "MAP"])
            self.logging_enabled = True
            self.log_btn.config(text="⏹ Logging")
            self.log(f"Logging to {fname}")
        else:
            if self.csv_file:
                self.csv_file.close()
            self.logging_enabled = False
            self.log_btn.config(text="💾 Log")
            self.log("Logging stopped")

    def send_cmd(self, cmd):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.write(cmd.encode())

    def read_loop(self):
        while self.running and self.connected:
            try:
                if self.serial_port and self.serial_port.in_waiting:
                    line = self.serial_port.readline().decode("utf-8", errors="replace").strip()
                    if line:
                        self.root.after(0, self.process_line, line)
                else:
                    time.sleep(0.05)
            except Exception as e:
                self.root.after(0, self.log, f"Serial error: {e}")
                self.root.after(0, self.disconnect)
                break

    def process_line(self, line):
        self.log_text.insert(tk.END, line + "\n")
        self.log_text.see(tk.END)

        if "RPM" in line and ":" in line:
            try:
                val = int(line.split(":")[1].strip().split()[0])
                self.data["RPM"].append(val)
                self.sample_count += 1
                self.time_data.append(self.sample_count)
                self.gauge_labels["RPM"].config(text=str(val))
                self.update_graphs()
            except:
                pass

        for key, label in [("SPEED", "SPEED"), ("TPS", "TPS"), ("ECT", "ECT"),
                           ("IAT", "IAT"), ("VOLT", "VOLT"), ("Gear", "GEAR"),
                           ("MAP", "MAP")]:
            if key in line and ":" in line:
                try:
                    parts = line.split(":")[1].strip().split()
                    val = parts[0] if parts else "?"
                    if key == "VOLT":
                        unit = "V" if "V" not in val else ""
                    elif key == "MAP":
                        unit = " kPa" if "kPa" not in val else ""
                    elif key == "SPEED":
                        unit = " km/h" if "km/h" not in val else ""
                    elif key in ("ECT", "IAT"):
                        unit = "°C" if "C" not in val else ""
                    elif key == "TPS":
                        unit = "%" if "%" not in val else ""
                    else:
                        unit = ""
                    display = f"{val}{unit}"
                    self.gauge_labels[label].config(text=display)
                except:
                    pass

        if "Engine:" in line and "RPM" in line:
            try:
                rpm_str = line.split(":")[1].strip().split()[0]
                rpm = int(rpm_str)
                self.data["RPM"].append(rpm)
                self.sample_count += 1
                self.time_data.append(self.sample_count)
                self.gauge_labels["RPM"].config(text=rpm_str)
                self.update_graphs()
            except:
                pass

        if "Coolant:" in line:
            try:
                temp = line.split(":")[1].strip().split()[0]
                self.gauge_labels["ECT"].config(text=temp + "°C")
            except:
                pass

        if "Gear:" in line:
            try:
                g = line.split(":")[1].strip()
                self.gauge_labels["GEAR"].config(text=g)
            except:
                pass

    def update_graphs(self):
        if len(self.time_data) < 2:
            return
        t = list(self.time_data)
        self.rpm_line.set_data(t, list(self.data["RPM"]))
        self.ax1.relim()
        self.ax1.autoscale_view()

        t2 = list(self.time_data)
        self.tps_line.set_data(t2, list(self.data["TPS"]))
        self.ect_line.set_data(t2, list(self.data["ECT"]))
        self.ax2.relim()
        self.ax2.autoscale_view()

        self.canvas.draw_idle()

    def poll_serial(self):
        if self.connected and self.running:
            self.send_cmd("s")
        self.root.after(500, self.poll_serial)

    def log(self, msg):
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert(tk.END, f"[{timestamp}] {msg}\n")
        self.log_text.see(tk.END)

if __name__ == "__main__":
    root = tk.Tk()
    app = SDSDashboard(root)
    root.mainloop()
