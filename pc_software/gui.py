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

# Assuming send_file.py exists and send_image is callable
from send_file import send_image

# Configure serial port
try:
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
except serial.SerialException as e:
    print(f"Could not open serial port /dev/ttyACM0: {e}")
    print("Serial communication will be disabled.")
    ser = None

# Path to the RTT log file
PATH = "RTT.log"

def ansi_to_html_with_cleanup(text):
    """
    Converts ANSI escape codes in text to HTML for display in QTextEdit.
    Removes unsupported ANSI sequences and ensures proper span closing.
    """
    # Remove non-color ANSI sequences (cursor moves, clears, etc.)
    # Only keep color reset (\x1b[0m) and green bold (\x1b[1;32m)
    text = re.sub(r'\x1b\[(?:[0-9;?]*[A-Za-z])', 
                  lambda m: '' if m.group(0) not in ['\x1b[0m', '\x1b[1;32m'] else m.group(0), text)

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

def parse_json_from_line(line: str):
    """
    Attempts to find and parse a JSON object from a string, ignoring leading junk.
    Assumes JSON starts with '{' or '['.
    """
    # Regex to find a JSON object or array at any position
    # This pattern looks for the first occurrence of '{...}' or '[...]'
    # It's a simplified regex and might need refinement for very complex/nested JSON scenarios
    json_match = re.search(r'(\{.*\}|\[.*\])', line)
    if json_match:
        json_str = json_match.group(0)
        try:
            parsed_json = json.loads(json_str)
            return parsed_json
        except json.JSONDecodeError:
            print(f"DEBUG: Could not decode JSON from matched string: {json_str}")
            return None
    return None # No JSON object found in the line

def run_python_recognition(filepath: str):
    """
    Runs the 'recognition.py' script as a subprocess and waits for it to finish.
    Captures its output and prints it.
    """
    print("Starting recognition.py script...")
    try:
        # The command to execute. 'python' is the interpreter, 'recognition.py' is the script.
        # Using a list is safer to avoid shell injection.
        result = subprocess.run(
            ["python", "recognition.py", filepath],
            capture_output=True,  # Capture stdout and stderr
            text=True,            # Decode output as text
            check=True            # Raise an error if recognition.py exits with a non-zero code
        )

        print("\nRecognition script finished successfully!")
        print("--- Script Output (STDOUT) ---")
        print(result.stdout)
        if result.stderr:
            print("--- Script Errors (STDERR) ---")
            print(result.stderr)

    except FileNotFoundError:
        print("\nError: 'python' command not found. Make sure Python is installed and in your PATH.")
    except subprocess.CalledProcessError as e:
        print(f"\nError: 'recognition.py' exited with a non-zero status code: {e.returncode}")
        print("--- Script Output (STDOUT) ---")
        print(e.stdout)
        print("--- Script Errors (STDERR) ---")
        print(e.stderr)
    except Exception as e:
        print(f"\nAn unexpected error occurred: {e}")

class SerialShellReader(QThread):
    """
    Reads lines from the serial port and emits them.
    """
    received = pyqtSignal(str)

    def run(self):
        if not ser:
            return
        while True:
            try:
                if ser.in_waiting:
                    # Read until newline, decode, strip whitespace
                    data = ser.readline().decode(errors="ignore").strip()
                    if data:
                        self.received.emit(data)
                else:
                    # Give up control to other threads
                    time.sleep(0.05) 
            except Exception as e:
                print(f"Serial read error: {e}")
                time.sleep(1) # Wait before retrying after an error

class RTTShellReader(QThread):
    """
    Reads new lines from RTT.log, parses JSON, and emits it.
    Handles junk characters before JSON.
    """
    json_received = pyqtSignal(dict) # Emits parsed JSON as a dictionary
    log_line_received = pyqtSignal(str) # Emits raw log lines for display

    def run(self):
        try:
            # Open file and seek to the end
            # This ensures we only read new data appended after the GUI starts
            with open(PATH, 'r', encoding='utf-8', errors='ignore') as file:
                file.seek(0, os.SEEK_END)
                while True:
                    line = file.readline()
                    if not line:
                        # No new line, wait a bit
                        time.sleep(0.1)
                        continue

                    # Emit the raw line for display in the shell output (optional)
                    # self.log_line_received.emit(line.strip())

                    # Attempt to parse JSON from the line
                    parsed_data = parse_json_from_line(line.strip())
                    if parsed_data:
                        self.json_received.emit(parsed_data)
                        print(parsed_data)
                    # else:
                        # print(f"DEBUG: No valid JSON found or parsed from line: {line.strip()}")

        except FileNotFoundError:
            print(f"Error: RTT.log not found at {PATH}. RTT reading disabled.")
            self.log_line_received.emit(f"<span style='color:red;'>Error: RTT.log not found at {PATH}.</span>")
        except Exception as e:
            print(f"Error reading RTT.log: {e}")
            self.log_line_received.emit(f"<span style='color:red;'>Error reading RTT.log: {e}</span>")
            time.sleep(1) # Wait before retrying

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
            "Ultrasonic": (0, 5000),  # 0.000–5.000 m, represented as integers for slider (mm)
            "Accel": (0, 100),        # 0.00–1.00, represented as integers for slider (*100)
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
        layout.addWidget(QLabel("Serial Shell Output (and RTT Log):"))
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

        # Start serial reader thread
        self.serial_thread = SerialShellReader()
        self.serial_thread.received.connect(self.update_shell_output)
        self.serial_thread.start()

        # Start RTT log reader thread
        self.rtt_thread = RTTShellReader()
        self.rtt_thread.json_received.connect(self.handle_rtt_json)
        self.rtt_thread.log_line_received.connect(self.update_shell_output) # Display raw RTT lines too
        self.rtt_thread.start()

    def slider_changed(self, sensor: str, value: int, label: QLabel):
        """Updates the slider label text based on the sensor type and value."""
        if sensor == "Accel":
            label.setText(f"Magnetometer (normalized²): {value / 100:.2f}")
        elif sensor == "Ultrasonic":
            label.setText(f"Ultrasonic (m): {value / 1000:.3f}")

    def send_single_threshold(self, sensor: str):
        """Sends a single sensor threshold command over serial."""
        value = self.sliders[sensor].value()
        if sensor == "Accel":
            value = value / 100.0 # Convert integer slider value back to float
        elif sensor == "Ultrasonic":
            value = value / 1000.0 # Convert integer slider value back to float

        flag = "u" if sensor == "Ultrasonic" else "m"
        # The command format should be `sensor <flag> <value>`
        # For example: `sensor u 1.500` or `sensor m 0.75`
        msg = f"sensor {flag} {value:.3f}\n" # Format value to 3 decimal places for consistency
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
        """Opens a file dialog for selecting an image to send."""
        options = QFileDialog.Options()
        file_name, _ = QFileDialog.getOpenFileName(self, "Select File to Send", "",
                                                    "Image Files (*.jpg *.jpeg *.png);;All Files (*)", options=options)
        if file_name:
            print(f"Selected file: {file_name}")
            self.send_file_to_dashboard(file_name)
        else:
            print("File selection cancelled.")

    def send_file_to_dashboard(self, file_path: str):
        """
        Sends the specified image file to the dashboard.
        Note: The original code passed a hardcoded "bounding_boxed.jpg".
        This is updated to use the selected file_path.
        """
        print(f"send_file_to_dashboard() called with path: {file_path}")
        # Assuming send_image function handles the actual sending logic
        try:
            run_python_recognition(file_path)
            send_image("bounding_boxed.jpg")
            self.show_message_box("Success", f"File '{os.path.basename(file_path)}' sent to dashboard.")
        except Exception as e:
            self.show_message_box("Error", f"Failed to send file: {e}")
            print(f"Error sending image: {e}")

    def run_serial_command(self):
        """Sends a command from the shell input to the serial port."""
        cmd = self.shell_input.text().strip()
        if not cmd:
            return
        
        # Display the command in the shell output
        self.shell_output.append(f"<span style='color:#0000FF;'>&gt; {cmd}</span>") # Blue for user input

        if ser:
            try:
                ser.write((cmd + "\n").encode())
            except Exception as e:
                self.shell_output.append(f"<span style='color:red;'>Error writing to serial: {e}</span>")
        else:
            self.shell_output.append("<span style='color:orange;'>Serial port not connected. Command not sent.</span>")
        self.shell_input.clear()

    def update_shell_output(self, text: str):
        """
        Appends new text (from serial or RTT raw lines) to the shell output.
        Applies ANSI to HTML conversion.
        """
        html = ansi_to_html_with_cleanup(text)
        # Use <pre> tag for preformatted text (monospace font, whitespace preserved)
        html_formatted = f'<pre style="font-family: monospace; white-space: pre-wrap; margin: 0;">{html}</pre>'
        self.shell_output.append(html_formatted)
        # Scroll to the bottom
        self.shell_output.verticalScrollBar().setValue(self.shell_output.verticalScrollBar().maximum())

    def handle_rtt_json(self, data: dict):
        """
        Slot to handle parsed JSON data received from the RTT log.
        This is where you'd process the JSON data (e.g., update sensor readings,
        display location data, etc.).
        For demonstration, it appends the JSON to the shell output.
        """
        # Append the parsed JSON in a distinct color for easy identification
        json_pretty = json.dumps(data, indent=2)
        self.shell_output.append(f"<span style='color:#800080;'>--- RTT JSON START ---</span>") # Purple for JSON
        self.shell_output.append(f'<pre style="font-family: monospace; white-space: pre-wrap; margin: 0; color:#800080;">{json_pretty}</pre>')
        self.shell_output.append(f"<span style='color:#800080;'>--- RTT JSON END ---</span>")
        self.shell_output.verticalScrollBar().setValue(self.shell_output.verticalScrollBar().maximum())

        # Here you would typically extract specific values from 'data'
        # For example, if your JSON is {"loc_x": 100, "loc_y": 200}:
        # if "loc_x" in data and "loc_y" in data:
        #     self.update_location_display(data["loc_x"], data["loc_y"])
        # print(f"Received JSON from RTT: {data}")


    def show_message_box(self, title: str, message: str):
        """Helper function to display a QMessageBox."""
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