import sys
import json
import serial
import time
import threading
from PyQt5.QtCore import Qt, pyqtSignal, QObject, QTimer
from PyQt5.QtWidgets import (
    QApplication, QWidget, QLabel, QVBoxLayout, QHBoxLayout,
    QPushButton
)
from PyQt5.QtGui import QFont
import pyqtgraph as pg


# ---------------------------------------------------------
# DATA FIXING
# ---------------------------------------------------------
def normalize(v):
    """Fix corrupt floats like 2.5e38, None, strings, etc."""
    try:
        v = float(v)
    except:
        return 0

    # Corrupted NRF values
    if v > 1e6 or v < -1e6:
        return 0

    if v < 0:
        return 0

    if v > 255:
        return 255

    return int(v)


def valid_signal(counts):
    """Reject all noise-only or corrupted sweeps."""
    if len(counts) != 8:
        return False

    if all(c == 0 for c in counts):
        return False

    avg = sum(counts) / 8

    if avg < 5:               # almost no RF signal
        return False

    return True


def detect_drone(counts):
    """Improved detection logic to avoid false alarms."""
    avg = sum(counts) / 8
    peak = max(counts)
    variation = peak - min(counts)

    # True drone signature
    if avg > 40 and variation > 30 and valid_signal(counts):
        return True

    return False


# ---------------------------------------------------------
# SERIAL WORKER THREAD
# ---------------------------------------------------------
class SerialThread(QObject):
    new_counts = pyqtSignal(list)

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.running = False

    def start(self):
        self.running = True
        threading.Thread(target=self.read_loop, daemon=True).start()

    def stop(self):
        self.running = False

    def read_loop(self):
        try:
            ser = serial.Serial(self.port, 115200, timeout=0.05)
            time.sleep(1)
        except Exception as e:
            return

        while self.running:
            try:
                raw = ser.readline().decode(errors="ignore").strip()
                if not raw:
                    continue

                # Parse JSON
                try:
                    data = json.loads(raw)
                except:
                    continue

                if "counts" not in data:
                    continue

                cleaned = [normalize(v) for v in data["counts"]]
                self.new_counts.emit(cleaned)

            except:
                continue


# ---------------------------------------------------------
# GUI
# ---------------------------------------------------------
class DroneGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("NRF Drone Detector — Fast Mode")
        self.resize(700, 500)

        layout = QVBoxLayout()

        title = QLabel("Real-Time NRF Drone Detector")
        title.setFont(QFont("Arial", 22))
        title.setAlignment(Qt.AlignCenter)
        layout.addWidget(title)

        # Buttons
        btn_row = QHBoxLayout()
        self.btn_start = QPushButton("Start")
        self.btn_stop = QPushButton("Stop")
        self.btn_stop.setEnabled(False)
        btn_row.addWidget(self.btn_start)
        btn_row.addWidget(self.btn_stop)
        layout.addLayout(btn_row)

        # Status label
        self.status_lbl = QLabel("STATUS: Waiting…")
        self.status_lbl.setFont(QFont("Arial", 18))
        self.status_lbl.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.status_lbl)

        # Plot
        self.graph = pg.PlotWidget()
        self.graph.setYRange(0, 255)
        self.bars = pg.BarGraphItem(x=list(range(8)), height=[0]*8, width=0.7)
        self.graph.addItem(self.bars)
        layout.addWidget(self.graph)

        self.setLayout(layout)

        # Data buffer (helps reduce GUI load)
        self.last_counts = [0]*8

        # GUI update timer every 150 ms
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_plot)
        self.timer.start(150)

        # Serial worker
        self.worker = None
        self.btn_start.clicked.connect(self.start_serial)
        self.btn_stop.clicked.connect(self.stop_serial)

    def start_serial(self):
        self.worker = SerialThread("COM13")
        self.worker.new_counts.connect(self.on_new_counts)
        self.worker.start()
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)

    def stop_serial(self):
        if self.worker:
            self.worker.stop()
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)

    def on_new_counts(self, counts):
        """Background thread gives us new samples."""
        self.last_counts = counts

    def update_plot(self):
        """GUI refresh (fast and smooth)."""
        self.bars.setOpts(height=self.last_counts)

        if detect_drone(self.last_counts):
            self.status_lbl.setText("⚠ DRONE DETECTED!")
            self.status_lbl.setStyleSheet("color:red;")
            QApplication.beep()
        else:
            self.status_lbl.setText("✔ No Drone")
            self.status_lbl.setStyleSheet("color:green;")


# ---------------------------------------------------------
# MAIN
# ---------------------------------------------------------
if __name__ == "__main__":
    app = QApplication(sys.argv)
    gui = DroneGUI()
    gui.show()
    sys.exit(app.exec_()) explain the code block wise what is this code