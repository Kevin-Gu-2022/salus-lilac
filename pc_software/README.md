# PC Software
The PC software is responsible for communicating with the dashboard as well as 
setting sensor thresholds through a GUI.

## Features
- Setting sensor thresholds for the ultrasonic sensor and magnetometer
- Sending events and sensor data to a web dashboard - TagoIO.
- Sending an image to dashboard. Due to technical difficulties with Zephyr, the
camera node was not implemented, but the image recognition and uploading to 
dashboard is still present.

## Installation
Run the following command:
```
pip install -r requirements.txt
```
## Instructions
1. Before starting the GUI, start a `JLinkRTTViewer` process and begin terminal
logging to a file called `RTT.log`.
2. Start the GUI by running:
```
python gui.py
```
3. Interface with shell inside GUI.


- TAMPERING
- PRESENCE
- SUCCESS
- FAIL