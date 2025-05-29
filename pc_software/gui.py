import sys
import os
import serial
import threading
import json
import time
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QLabel, QSlider,
    QHBoxLayout, QPushButton, QTextEdit, QFileDialog, QMessageBox
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtGui import QFont

from send_file import send_image
from recognition import detect_objects

# Configure serial port (adjust as needed for your setup)
# Note: This line is kept for completeness but the current SerialReader
# reads from RTT.log, not directly from the serial port.
try:
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
except serial.SerialException as e:
    print(f"Could not open serial port /dev/ttyACM0: {e}")
    print("Serial communication will be disabled. Only RTT.log reading will function.")
    ser = None # Set ser to None if port cannot be opened

PATH = "RTT.log" # Path to the log file to be tailed

class SerialReader(QThread):
    """
    A QThread subclass that continuously reads new lines from the RTT.log file.
    It processes each line, attempting to parse it as a JSON string.
    Only valid JSON strings are emitted via the 'received' signal.
    """
    received = pyqtSignal(str) # Signal to emit received data to the main GUI thread

    def run(self):
        """
        Reads the RTT.log file, tailing it for new lines.
        If a new line is found, it attempts to decode it as JSON.
        Valid JSON strings are then emitted.
        """
        try:
            # Open the RTT.log file in read mode
            with open(PATH, 'r') as file:
                # Seek to the end of the file to start 'tailing'
                file.seek(0, 2)
                print(f"Tailing log file: {PATH}")

                while True:
                    line = file.readline() # Read a new line

                    if not line:
                        # If no new line is found, wait a short period before trying again
                        time.sleep(0.2)
                        continue

                    # Attempt to parse the line as a JSON object
                    try:
                        # Strip whitespace (like newline characters) from the line
                        entry = json.loads(line.strip())
                        # If parsing is successful, it's valid JSON.
                        # Emit the JSON string (re-dumped for consistency) to the GUI.
                        self.received.emit(json.dumps(entry))
                        # print(f"Emitted valid JSON: {line.strip()}") # For debugging
                    except json.JSONDecodeError:
                        # If the line is not valid JSON, print a message and skip it
                        print(f"Invalid JSON format, skipping line: {line.strip()}")
                    except Exception as e:
                        # Catch any other unexpected errors during processing
                        print(f"[ERROR] An unexpected error occurred while processing line: {e} - Line: {line.strip()}")
        except FileNotFoundError:
            print(f"Error: The file '{PATH}' was not found.")
        except Exception as e:
            print(f"An error occurred in SerialReader thread: {e}")

class SensorGUI(QWidget):
    """
    The main GUI window for displaying sensor thresholds and received data.
    It allows users to set thresholds for various sensors and displays
    incoming JSON data from the SerialReader thread.
    """
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Sensor Thresholds")
        self.resize(700, 400) # Set initial window size

        layout = QVBoxLayout() # Main vertical layout for the window
        self.sliders = {} # Dictionary to store QSlider widgets
        self.labels = {} # Dictionary to store QLabel widgets for slider values

        font = QFont()
        font.setPointSize(12) # Set a common font size for better readability

        # Define sensors and their display labels
        sensors = ["Ultrasonic", "Accel_X", "Accel_Y", "Accel_Z"]
        sensor_labels = ["Ultrasonic (cm)", "Accel_X (m/s²)", "Accel_Y (m/s²)", "Accel_Z (m/s²)"]
        self.sensors = sensors # Store sensor names

        # Define ranges for each sensor's slider
        sensor_ranges = {
            "Ultrasonic": (0, 500),   # cm
            "Accel_X": (-20, 20),     # m/s²
            "Accel_Y": (-20, 20),
            "Accel_Z": (-20, 20),
        }

        # Create GUI elements for each sensor
        for index, sensor in enumerate(sensors):
            min_val, max_val = sensor_ranges[sensor]
            hlayout = QHBoxLayout() # Horizontal layout for each sensor row

            # Label to display current slider value
            label = QLabel(f"{sensor_labels[index]}: 0")
            label.setFont(font)

            # Slider for setting the threshold
            slider = QSlider(Qt.Horizontal)
            slider.setMinimum(min_val)
            slider.setMaximum(max_val)
            slider.setValue(0) # Initial slider value
            slider.setFont(font)
            # Connect slider's valueChanged signal to the slider_changed method
            slider.valueChanged.connect(lambda val, s=sensor, l=label: self.slider_changed(s, val, l))

            # Button to send the current slider value
            send_button = QPushButton("Send")
            send_button.setFont(font)
            # Connect send button's clicked signal to the send_single_threshold method
            send_button.clicked.connect(lambda _, s=sensor: self.send_single_threshold(s))

            # Add widgets to the horizontal layout
            hlayout.addWidget(label)
            hlayout.addWidget(slider)
            hlayout.addWidget(send_button)

            # Add the horizontal layout to the main vertical layout
            layout.addLayout(hlayout)
            self.sliders[sensor] = slider # Store slider reference
            self.labels[sensor] = label   # Store label reference

        # Add a "Send File" button
        self.send_file_button = QPushButton("Send Image to Dashboard")
        self.send_file_button.setFont(font)
        self.send_file_button.clicked.connect(self.open_file_dialog)
        layout.addWidget(self.send_file_button)

        # Text box to display received serial data
        self.rx_display = QTextEdit()
        self.rx_display.setReadOnly(True) # Make it read-only
        self.rx_display.setFont(font)
        layout.addWidget(self.rx_display)

        self.setLayout(layout) # Set the main layout for the window

        # Initialize and start the serial reading thread
        self.serial_thread = SerialReader()
        # Connect the 'received' signal from the thread to the handle_serial_data method
        self.serial_thread.received.connect(self.handle_serial_data)
        self.serial_thread.start() # Start the thread

    def slider_changed(self, sensor, value, label):
        """
        Updates the QLabel text when a slider's value changes.
        """
        if "Accel" in sensor:
            label.setText(f"{sensor} (m/s²): {value}")
        else:
            label.setText(f"{sensor} (cm): {value}")

    def send_single_threshold(self, sensor):
        """
        Sends the current value of a specific sensor's slider over serial.
        This function will only work if the serial port was successfully opened.
        """
        value = self.sliders[sensor].value()
        msg = f"{sensor}:{value}\n"
        print(f"Attempting to send: {msg.strip()}")
        if ser: # Check if serial port is open
            try:
                ser.write(msg.encode()) # Encode and send the message
            except serial.SerialException as e:
                print(f"Error sending data over serial: {e}")
                self.show_message_box("Serial Error", f"Could not send data: {e}")
        else:
            print("Serial port not available. Data not sent.")
            self.show_message_box("Serial Not Connected", "Serial port is not connected. Cannot send data.")

    def open_file_dialog(self):
        """
        Opens a file dialog to allow the user to select a file.
        If a file is selected, its path is passed to the send_file() method.
        """
        options = QFileDialog.Options()
        # options |= QFileDialog.DontUseNativeDialog # Uncomment for non-native dialog
        file_name, _ = QFileDialog.getOpenFileName(self, "Select File to Send", "",
                                                  "All Files (*);;Text Files (*.txt);;JSON Files (*.json)", options=options)
        if file_name:
            print(f"Selected file: {file_name}")
            # Call the new send_file function with the selected file path
            self.send_file(file_name)
        else:
            print("File selection cancelled.")

    def send_file(self, file_path):
        """
        This function is called after a file path is obtained from the file dialog.
        """
        print(f"send_file() called with path: {file_path}")
        #  Send the image to dashboard
        rel_path = os.path.relpath(file_path)
        detect_objects(rel_path)
        send_image("bounding_boxed.jpg")


    def handle_serial_data(self, data):
        """
        Callback function that receives data from the SerialReader thread.
        Appends the received data to the text display.
        """
        # The SerialReader now ensures that 'data' is a valid JSON string.
        if data.startswith("{"):
            self.rx_display.append(f"{data}")
        else:
            # This case should ideally not be hit if SerialReader only emits JSON
            print(f"Received non-JSON data from thread (should not happen): {data}")

    def show_message_box(self, title, message):
        """
        Displays a simple message box to the user instead of alert().
        """
        msg_box = QMessageBox()
        msg_box.setWindowTitle(title)
        msg_box.setText(message)
        msg_box.setIcon(QMessageBox.Information)
        msg_box.setStandardButtons(QMessageBox.Ok)
        msg_box.exec_()


if __name__ == "__main__":
    # Create the QApplication instance
    app = QApplication(sys.argv)
    # Create an instance of the SensorGUI window
    window = SensorGUI()
    # Show the window
    window.show()
    # Start the Qt event loop
    sys.exit(app.exec_())
