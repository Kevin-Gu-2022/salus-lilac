import sys
import os
import serial
import json
import time
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QLabel, QSlider,
    QHBoxLayout, QPushButton, QTextEdit, QFileDialog, QMessageBox
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtGui import QFont

from send_file import send_image

# Configure serial port
try:
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
except serial.SerialException as e:
    print(f"Could not open serial port /dev/ttyACM0: {e}")
    print("Serial communication will be disabled. Only RTT.log reading will function.")
    ser = None

PATH = "RTT.log"

class SerialReader(QThread):
    received = pyqtSignal(str)

    def run(self):
        try:
            with open(PATH, 'r') as file:
                file.seek(0, 2)
                print(f"Tailing log file: {PATH}")
                while True:
                    line = file.readline()
                    if not line:
                        time.sleep(0.2)
                        continue
                    try:
                        entry = json.loads(line.strip())
                        self.received.emit(json.dumps(entry))
                    except json.JSONDecodeError:
                        print(f"Invalid JSON format, skipping line: {line.strip()}")
                    except Exception as e:
                        print(f"[ERROR] Unexpected error while processing line: {e} - Line: {line.strip()}")
        except FileNotFoundError:
            print(f"Error: The file '{PATH}' was not found.")
        except Exception as e:
            print(f"An error occurred in SerialReader thread: {e}")

class SensorGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Sensor Thresholds")
        self.resize(700, 400)

        layout = QVBoxLayout()
        self.sliders = {}
        self.labels = {}

        font = QFont()
        font.setPointSize(12)

        sensors = ["Ultrasonic", "Accel"]
        sensor_labels = ["Ultrasonic (m)", "Accel (normalized²)"]
        self.sensors = sensors

        sensor_ranges = {
            "Ultrasonic": (0, 5000),  # Represent 0.000–5.000 m
            "Accel": (0, 100),        # Represent 0.00–1.00
        }

        for index, sensor in enumerate(sensors):
            min_val, max_val = sensor_ranges[sensor]
            hlayout = QHBoxLayout()

            label = QLabel(f"{sensor_labels[index]}: 0")
            label.setFont(font)

            slider = QSlider(Qt.Horizontal)
            slider.setMinimum(min_val)
            slider.setMaximum(max_val)
            slider.setValue(0)
            slider.setFont(font)
            slider.valueChanged.connect(lambda val, s=sensor, l=label: self.slider_changed(s, val, l))

            send_button = QPushButton("Send")
            send_button.setFont(font)
            send_button.clicked.connect(lambda _, s=sensor: self.send_single_threshold(s))

            hlayout.addWidget(label)
            hlayout.addWidget(slider)
            hlayout.addWidget(send_button)

            layout.addLayout(hlayout)
            self.sliders[sensor] = slider
            self.labels[sensor] = label

        self.send_file_button = QPushButton("Send Image to Dashboard")
        self.send_file_button.setFont(font)
        self.send_file_button.clicked.connect(self.open_file_dialog)
        layout.addWidget(self.send_file_button)

        self.rx_display = QTextEdit()
        self.rx_display.setReadOnly(True)
        self.rx_display.setFont(font)
        layout.addWidget(self.rx_display)

        self.setLayout(layout)

        self.serial_thread = SerialReader()
        self.serial_thread.received.connect(self.handle_serial_data)
        self.serial_thread.start()

    def slider_changed(self, sensor, value, label):
        if sensor == "Accel":
            label.setText(f"{sensor} (normalized²): {value / 100:.2f}")
        else:
            label.setText(f"{sensor} (m): {value / 1000:.3f}")

    def send_single_threshold(self, sensor: str):
        value = self.sliders[sensor].value()
        if sensor == "Accel":
            value = value / 100
        elif sensor == "Ultrasonic":
            value = value / 1000

        flag = "u" if sensor == "Ultrasonic" else "m"
        msg = f"sensor {flag} {value}\n"
        print(f"Sending: {msg.strip()}")

        if ser:
            try:
                ser.write(msg.encode())
            except serial.SerialException as e:
                print(f"Error sending data over serial: {e}")
                self.show_message_box("Serial Error", f"Could not send data: {e}")
        else:
            print("Serial port not available. Data not sent.")
            self.show_message_box("Serial Not Connected", "Serial port is not connected. Cannot send data.")

    def open_file_dialog(self):
        options = QFileDialog.Options()
        file_name, _ = QFileDialog.getOpenFileName(self, "Select File to Send", "",
                                                  "All Files (*);;Text Files (*.txt);;JSON Files (*.json)", options=options)
        if file_name:
            print(f"Selected file: {file_name}")
            self.send_file(file_name)
        else:
            print("File selection cancelled.")

    def send_file(self, file_path):
        print(f"send_file() called with path: {file_path}")
        rel_path = os.path.relpath(file_path)
        send_image("bounding_boxed.jpg")

    def handle_serial_data(self, data):
        if data.startswith("{"):
            self.rx_display.append(f"{data}")
        else:
            print(f"Received non-JSON data from thread (should not happen): {data}")

    def show_message_box(self, title, message):
        msg_box = QMessageBox()
        msg_box.setWindowTitle(title)
        msg_box.setText(message)
        msg_box.setIcon(QMessageBox.Information)
        msg_box.setStandardButtons(QMessageBox.Ok)
        msg_box.exec_()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SensorGUI()
    window.show()
    sys.exit(app.exec_())
