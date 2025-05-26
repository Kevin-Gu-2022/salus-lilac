# CSSE4011 Prac 1 
## Introduction to Zephyr RTOS and Bootloader 
### Xinlin Zhong, 48018061

#### **Design Task 1: Random Number Generator**
#### Overview 
This task implements a **multithreaded random number generator (RNG)** using **Zephyr RTOS**. It utilizes Zephyr's **hardware RNG API** to generate random numbers and display them every 2s via the
serial output.

#### Features 
- **RNG Thread**: generates a random 8-digit number when signaled from `semaphore`.
- **Display Thread**: retrieves from `message queue` and displays the number via UART every 2s.

#### **Design Task 2: RGB LED Control**
#### Overview 
This task implements a **multithreaded RGB LED controller** using Zephyr RTOS. It cycles through the 8 primary colour every 2s using an **Grove Chainable RGB LED**. It uses bit manipulation to control the colour of the RGB LED. 

#### Features 
- **RGB LED Thread**: controls the colour of the RGB LED when signalling from `semaphore`.
- **Control Thread**: sets(updates) the colour of the RGB LED every 2s based on the below sequences.

| Color   |
|---------|
| Black   |
| Blue    |
| Green   |
| Cyan    |
| Red     |
| Magenta |
| Yellow  |
| White   |

#### **Design Task 3: Command Line Interface Shell**
#### Overview 
This task implements a CLI shell for `B-L475E-IOT` Platform via serial terminal.

#### Features 
- **System Timer Command**: displays system uptime in `seconds` or formatted as `HH:MM:SS` based on specified commands.
- **LED Control Command**: sets or toggles on board LEDs using a bitmask based on specified commands.
- **Logging System:**: displays debug, info, warning and error messages using `Zephyr's Logging API`.
- Allows runtime filtering of log messages via shell commands. 

#### **Folder structure**
| csse4011/  
        │── repo/  
                │── mycode/  
                        │── apps/  
                                │── prac1/  
                                        │── task1/  
                                                │── boards/  
                                                        │── disco_l475_iot1.overlay  
                                                │── src/  
                                                        │── main.c  
                                                │── sysbuild/  
                                                        │── mcuboot.conf  
                                                        │── mcuboot.overlay  
                                                 │── CMakeLists.txt  
                                                │── prj.conf  
                                                │── README.rst  
                                                │── sample.yaml  
                                                │── sysbuild.conf  
                                        │── task2/  
                                                │── boards/  
                                                        │── disco_l475_iot1.overlay  
                                                │── src/  
                                                        │── main.c  
                                                │── sysbuild/  
                                                        │── mcuboot.conf  
                                                        │── mcuboot.overlay  
                                                 │── CMakeLists.txt  
                                                │── prj.conf  
                                                │── README.rst  
                                                │── sample.yaml  
                                                │── sysbuild.conf  
                                        │── task3/  
                                                │── boards/  
                                                        │── disco_l475_iot1.overlay  
                                                │── src/  
                                                        │── main.c  
                                                │── sysbuild/  
                                                        │── mcuboot.conf  
                                                        │── mcuboot.overlay  
                                                 │── CMakeLists.txt  
                                                │── prj.conf  
                                                │── README.rst  
                                                │── sample.yaml  
                                                │── sysbuild.conf  
                                │── README.md  

### How to Build and Run
**Build**: 
- Task1: `west build -b disco_l475_iot1 --sysbuild mycode/apps/prac1/task1 --pristine`
- Task2: `west build -b disco_l475_iot1 --sysbuild mycode/apps/prac1/task2 --pristine`
- Task3: `west build -b disco_l475_iot1 --sysbuild mycode/apps/prac1/task3 --pristine`

**Run**: `west flash --runner jlink`


