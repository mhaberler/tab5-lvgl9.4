# M5Stack Tab5 - LVGL 9.4 Networking Project

A PlatformIO-based firmware for the ESP32-P4 that integrates LVGL 9.4 GUI, WiFi connectivity, MQTT broker, and BLE scanning capabilities for the M5Stack Tab5 device.

## Overview

This project provides a complete embedded solution featuring:
- **LVGL 9.4 GUI** - Modern graphical interface with M5Stack display support
- **MQTT Broker** - Embedded MQTT broker with TCP and WebSocket support
- **BLE Scanner** - Bluetooth Low Energy device detection and advertisement parsing
- **WiFi Connectivity** - Station mode with mDNS service discovery
- **OTA Updates** - ESP-Hosted co-processor firmware updates

## Hardware Requirements

- **ESP32-P4 EVBoard** (M5Stack Tab5)
- SDIO2 WiFi interface on pins:
  - CLK: GPIO 12
  - CMD: GPIO 13
  - D0: GPIO 11
  - D1: GPIO 10
  - D2: GPIO 9
  - D3: GPIO 8
  - RST: GPIO 15

## Software Requirements

- [PlatformIO](https://platformio.org/)
- Python 3.x (for PlatformIO)
- ESP32 development tools

## Project Structure

```
├── include/
│   └── lv_conf.h           # LVGL configuration
├── lib/
│   ├── display/            # Display driver implementation
│   │   ├── display_driver.cpp
│   │   └── display_driver.h
│   └── ui/                 # LVGL UI components
│       ├── ui.c
│       └── ui.h
├── src/
│   ├── blescan.cpp         # BLE scanning and advertisement processing
│   ├── broker.hpp          # MQTT broker definitions
│   ├── fmicro.h            # Utility headers
│   ├── main.cpp            # Main application entry point
│   ├── mqtt.cpp            # Custom MQTT server implementation
│   └── ringbuffer.hpp      # Ring buffer for BLE data
├── partitions.csv          # Flash partition table
└── platformio.ini          # PlatformIO configuration
```

## Features

### MQTT Broker
- **TCP Server**: Port 1883
- **WebSocket Server**: Port 8883
- Tracks connected clients, subscriptions, and messages
- mDNS service advertisement (`picomqtt.local`)

### BLE Scanner
- Continuous BLE device scanning
- Parses device advertisements
- Extracts MAC addresses, RSSI, names, and manufacturer data
- JSON-based data format
- Ring buffer for advertisement queuing

### Display & UI
- LVGL 9.4 integration
- M5Stack M5Unified and M5GFX support
- Configurable rotation and brightness
- Custom UI components

### WiFi Management
- Station mode connection
- Status change monitoring
- mDNS hostname: `picomqtt`
- Automatic reconnection handling

## Environment Variables

Set these environment variables before building:

```bash
export WIFI_SSID2="your_wifi_ssid"
export WIFI_PASSWORD2="your_wifi_password"
```

## Building

```bash
# Clean build
pio run --target clean --environment esp32p4_pioarduino

# Build firmware
pio run --environment esp32p4_pioarduino

# Upload to device
pio run --target upload --environment esp32p4_pioarduino --upload-port /dev/cu.usbmodem12401

# Monitor serial output
pio device monitor --environment esp32p4_pioarduino
```

## Dependencies

- **[M5Unified](https://github.com/M5Stack/M5Unified.git)** - M5Stack device abstraction
- **[M5GFX](https://github.com/M5Stack/M5GFX.git)** - Graphics library
- **[LVGL v9.4.0](https://github.com/lvgl/lvgl)** - GUI library
- **[PicoMQTT](https://github.com/mlesniew/PicoMQTT)** - Lightweight MQTT broker
- **[PicoWebsocket](https://github.com/mlesniew/PicoWebsocket)** - WebSocket server
- **[ArduinoJson](https://github.com/bblanchon/ArduinoJson)** - JSON parsing

## Configuration

### Build Flags
- `CORE_DEBUG_LEVEL=3` - Verbose logging
- `CUSTOM_ARDUINO_LOOP_STACK_SIZE=32768` - Increased stack size
- `BOARD_HAS_PSRAM` - PSRAM support enabled
- `LV_CONF_INCLUDE_SIMPLE` - LVGL configuration

### Debug Settings
- **Debug Tool**: esp-builtin
- **Initial Break**: `tbreak setup`
- **Monitor Speed**: 115200 baud
- **Upload Speed**: 1500000 baud

## Usage

### Connecting to MQTT Broker

**TCP Connection:**
```bash
mosquitto_pub -h picomqtt.local -p 1883 -t "test/topic" -m "Hello"
```

**WebSocket Connection:**
```javascript
const client = new Paho.MQTT.Client("picomqtt.local", 8883, "/mqtt", "clientId");
client.connect({onSuccess: () => console.log("Connected!")});
```

### BLE Advertisement Format

BLE scan results are formatted as JSON:
```json
{
  "id": "AA:BB:CC:DD:EE:FF",
  "rssi": -65,
  "name": "Device Name",
  "manufacturerdata": "0102030405"
}
```

## Development

### Adding UI Elements
1. Modify [lib/ui/ui.c](lib/ui/ui.c) and [lib/ui/ui.h](lib/ui/ui.h)
2. Update display refresh in [main.cpp](src/main.cpp) `loop()`
3. Rebuild and upload

### Customizing MQTT Behavior
- Edit [mqtt.cpp](src/mqtt.cpp) to override `CustomMQTTServer` methods
- Available hooks: `on_connected`, `on_disconnected`, `on_subscribe`, `on_message`

### BLE Data Processing
- Modify [blescan.cpp](src/blescan.cpp) callback `onResult()` for custom parsing
- Adjust `BLE_ADV_QUEUELEN` for queue size

## Troubleshooting

### WiFi Connection Issues
- Verify environment variables are set correctly
- Check SDIO pin configuration matches hardware
- Monitor serial output for connection status

### Build Errors
- Ensure all submodules are initialized
- Update PlatformIO platform: `pio pkg update`
- Clean build directory: `pio run --target clean`

### Display Problems
- Verify M5Stack initialization in [main.cpp](src/main.cpp)
- Check LVGL buffer configuration in [lv_conf.h](include/lv_conf.h)
- Adjust brightness value in `setup()`

## Authors

Michael Haberler, based on https://github.com/fermintm/M5stack-TAB5-LVGL-9.4
## Acknowledgments

- ESP32-P4 platform by [pioarduino](https://github.com/pioarduino)
- LVGL team for the excellent GUI library
- M5Stack for hardware support libraries
