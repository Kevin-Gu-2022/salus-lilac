import sys
import os
import serial
import json
import re
import time
import subprocess
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QLabel, QSlider,
    QHBoxLayout, QPushButton, QFileDialog, QMessageBox,
    QTextEdit, QLineEdit
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtGui import QFont

from send_file import send_image

# Configure serial port
try:
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
except serial.SerialException as e:
    print(f"Could not open serial port /dev/ttyACM0: {e}")
    print("Serial communication will be disabled.")
    ser = None

PATH = "RTT.log"



def ansi_to_html_with_cleanup(text):
    # Remove non-color ANSI sequences (cursor moves, clears, etc.)
    text = re.sub(r'\x1b\[(?:[0-9;?]*[A-Za-z])', lambda m: '' if m.group(0) not in ['\x1b[0m', '\x1b[1;32m'] else m.group(0), text)

    # Map supported ANSI color codes to HTML
    ansi_colors = {
        '1;32': '<span style="color:green; font-weight:bold">',
        '0': '</span>',  # reset
    }

    def replace_color(match):
        code = match.group(1)
        return ansi_colors.get(code, '')

    text = re.sub(r'\x1b\[([0-9;]*)m', replace_color, text)

    # Close unclosed spans
    open_spans = text.count('<span')
    close_spans = text.count('</span>')
    if open_spans > close_spans:
        text += '</span>' * (open_spans - close_spans)

    return text

class SerialShellReader(QThread):
    received = pyqtSignal(str)

    def run(self):
        if not ser:
            return
        while True:
            try:
                if ser.in_waiting:
                    data = ser.readline().decode(errors="ignore").strip()
                    if data:
                        self.received.emit(data)
                else:
                    time.sleep(0.1)
            except Exception as e:
                print(f"Serial read error: {e}")
                time.sleep(1)

class SensorGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Sensor Thresholds + Serial Shell")
        self.resize(700, 500)

        layout = QVBoxLayout()
        self.sliders = {}
        self.labels = {}

        font = QFont()
        font.setPointSize(12)

        sensors = ["Ultrasonic", "Accel"]
        sensor_labels = ["Ultrasonic (m)", "Magnetometer (normalized²)"]
        self.sensors = sensors

        sensor_ranges = {
            "Ultrasonic": (0, 5000),  # 0.000–5.000 m
            "Accel": (0, 100),        # 0.00–1.00
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

        # Shell Output
        layout.addWidget(QLabel("Serial Shell Output:"))
        self.shell_output = QTextEdit()
        self.shell_output.setReadOnly(True)
        self.shell_output.setFont(font)
        layout.addWidget(self.shell_output)

        self.shell_input = QLineEdit()
        self.shell_input.setPlaceholderText("Enter command to send to /dev/ttyACM0 and press Enter")
        self.shell_input.setFont(font)
        self.shell_input.returnPressed.connect(self.run_serial_command)
        layout.addWidget(self.shell_input)

        self.setLayout(layout)

        self.serial_thread = SerialShellReader()
        self.serial_thread.received.connect(self.update_shell_output)
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

    def run_serial_command(self):
        cmd = self.shell_input.text().strip()
        if not cmd or not ser:
            return
        try:
            self.shell_output.append(f"> {cmd}")
            ser.write((cmd + "\n").encode())
        except Exception as e:
            self.shell_output.append(f"<span style='color:red;'>Error writing to serial: {e}</span>")
        self.shell_input.clear()

    def update_shell_output(self, text):
        html = ansi_to_html_with_cleanup(text)
        html = f'<pre style="font-family: monospace; white-space: pre-wrap;">{html}</pre>'
        self.shell_output.append(html)

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
