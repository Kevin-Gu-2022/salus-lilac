import sys
import serial
import threading
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QLabel, QSlider,
    QHBoxLayout, QPushButton, QTextEdit
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtGui import QFont

# Configure serial port
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)  # Adjust as needed

class SerialReader(QThread):
    received = pyqtSignal(str)

    def run(self):
        while True:
            if ser.in_waiting:
                data = ser.readline().decode(errors='ignore').strip()
                if data:
                    self.received.emit(data)

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

        sensors = ["Ultrasonic", "Accel_X", "Accel_Y", "Accel_Z"]
        sensor_labels = ["Ultrasonic (cm)", "Accel_X (m/s²)", "Accel_Y (m/s²)", "Accel_Z (m/s²)"]
        self.sensors = sensors

        sensor_ranges = {
            "Ultrasonic": (0, 500),   # cm
            "Accel_X": (-20, 20),     # m/s²
            "Accel_Y": (-20, 20),
            "Accel_Z": (-20, 20),
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

        # Text box to show received data
        self.rx_display = QTextEdit()
        self.rx_display.setReadOnly(True)
        self.rx_display.setFont(font)
        layout.addWidget(self.rx_display)

        self.setLayout(layout)

        # Start serial reading thread
        self.serial_thread = SerialReader()
        self.serial_thread.received.connect(self.handle_serial_data)
        self.serial_thread.start()

    def slider_changed(self, sensor, value, label):
        if "Accel" in sensor:
            label.setText(f"{sensor} (m/s²): {value}")
        else:
            label.setText(f"{sensor} (cm): {value}")

    def send_single_threshold(self, sensor):
        value = self.sliders[sensor].value()
        msg = f"{sensor}:{value}\n"
        print(msg.strip())
        ser.write(msg.encode())

    def handle_serial_data(self, data):
        # Only display JSON
        if (data[0] == "{"):
            self.rx_display.append(f"{data}")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SensorGUI()
    window.show()
    sys.exit(app.exec_())
