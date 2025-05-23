# CSSE4011 Prac 2
## Sensor Processing, Logging and Viewing
### Xinlin Zhong, 48018061

#### **Design Task 1: Real Time Clock (RTC)**
This task creats a library that initialises and uses the on-chip RTC (clock or counter).

#### **Design Task 2: Sensor Interfacing**
This task creates a library that uses threads to interface to sensors (temperature, humidity, pressure and magnetometer).

#### **Design Task 3:  Command Line Interface (CLI) Shell**
This task creats a CLI shell to control that use RTC and sensor libraries to read sensor value and read/wirte to RTC.

#### **Design Task 4:  Continuous Sampling**
This task creates a shell command (sample) that will enable continuous sampling (rate is set by sample
command) and show the selected sensor output in a JSON format.

#### **Design Task 5:  File Logging**
This task implement the Zephyr File System and include the shell commands to create files and folders and create shell command(s) to log the output of a selected sensor to a file system.

#### **Design Task 7:  Sensor Output Visualisation**
This task creates a desktop GUI program that can display the JSON sensor output from each platform
as a time-series graph.

#### **Folder structure**
| csse4011/  
        │── repo/  
                │── mycode/  
                        │── apps/  
                                │── prac2/  
                                        │── src/  
                                                │── main.c  
                                        │── prj.conf  
                                        │── README.md  
                                        │── sample.yaml  
                                        │── gui.py  
                        │── include/  
                                │── rtc_lib.h  
                                │── sensor_interface.h  
                        │── mylib/  
                                │── consumer.h  
                                │── humidity.h  
                                │── magnetometer.h  
                                │── pressure.h  
                                │── sensor_interface.h  
                                │── temperature.h  
           

### How to Build and Run
**Build**: 
`west build -b disco_l475_iot1 mycode/apps/prac2 --pristine`

**Run**: `west flash --runner jlink`

**Run GUI**:  `python gui.py`
