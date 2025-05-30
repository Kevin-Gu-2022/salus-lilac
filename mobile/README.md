# Mobile Node - Bluetooth Peripheral Display Interface

**Device:** Embedded screen-based mobile node (simulated phone)
---

## Project Overview

This project implements a **Bluetooth Low Energy (BLE) peripheral node** that simulates a mobile phone. It visually displays security-related information received from a **Base Node (BLE central)** over a custom protocol.

The mobile node listens for BLE connections, receives commands or status updates, and displays real-time feedback on a screen using **LVGL graphics**.

---

## Features

- **BLE Peripheral Role (NUS Service)**  
  - Waits for a connection from a central/base node
  - Accepts incoming data and triggers display logic

- **Visual Feedback on Screen**  
  Displays:
  - Number of passcode attempts
  - Passcode digits (masked/unmasked)
  - Status indicators (success, fail, warning, Bluetooth connection)
  - Lock icon and user prompts

- **Dynamic UI Components**  
  Based on received characters or formatted data, the system updates:
  - Labels (digit entry, attempts)
  - Image icons (lock, warning, Bluetooth, etc.)

- **Data Message Parsing**
  - `N`: Incorrect passcode (flashes lock image)
  - `Y`: Correct passcode (displays success view)
  - `F`: Too many failed attempts (displays warning view)
  - `X:YYYY`: Structured format where:
    - `X` = number of attempts
    - `YYYY` = 4-digit passcode characters or `-` for blank

---

## Bluetooth Behavior

- Uses **Zephyr’s NUS (Nordic UART Service)** for data transfer
- Starts advertising on boot
- Handles connection, disconnection, and reconnection (with 500ms delay)
- Semaphores used to trigger UI updates on connection state changes

---

## UI Components

Images and labels are loaded from assets:
- `bluetooth_img.c`
- `lock_img.c`
- `warning_img.c`
- `attempts_img.c`
- `pass_img.c`
- `enter_img.c`
- `dashline_img.c`

LVGL timers and flags manage dynamic visibility and timed effects.

## Thread Design

- `ble_thread`: Handles BLE stack initialization and advertising
- `init_interface_thread`: Initializes display and handles LVGL loop
- `update_interface_thread`: Parses BLE messages and updates UI
- `ui_setup_connection_thread`: Updates UI based on connection status

## Build Requirements

- Zephyr SDK and toolchain
- Display driver configured via Devicetree
- LVGL and Display subsystems enabled in `prj.conf`
- Bluetooth enabled with NUS support

### `prj.conf` (example essentials)
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_NUS=y
CONFIG_BT_DEVICE_NAME="MobileNode"
CONFIG_DISPLAY=y
CONFIG_LVGL=y
CONFIG_MAIN_STACK_SIZE=4096

## Build & Flash
**Build**: `west build -b m5stack_core2/esp32/procpu mycode/apps/pf/salus-lilac/mobile --pristine`

**Run**: `west flash --esp-baud-rate 115200`
