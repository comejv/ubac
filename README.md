# UBAC (Universal Box Ambient Control)

UBAC is a firmware for ESP32 designed to monitor multiple NTC temperature sensors and control a fan via PWM.

## Features
- **Temperature Monitoring:** Supports up to 10 NTC sensors via a CD74HC4067 multiplexer and ADS1115 ADC.
- **Fan Control:** PWM-based fan speed control (Skeleton implemented).
- **Web Server:** Integrated HTTP server for remote monitoring (Skeleton implemented).
- **Modular Design:** Easily extensible for additional sensors or actuators.

## Hardware Components
- **MCU:** ESPRESSIF ESP32-WROOM-32 on ESP32-DEVKITC V2 board
- **ADC:** TI ADS1115 (I2C)
- **MUX:** TI CD74HC4067 (Analog Multiplexer)
- **Sensors:** SEMITEC 104-JT (NTC)

## License
This project software is licensed under the **GNU General Public License v3.0**, and hardware designs are licensed under the **CERN Open Hardware Licence Version 2 - Strongly Reciprocal**.

## Development
This project uses the [ESP-IDF](https://github.com/espressif/esp-idf) framework.

### Building
```bash
idf.py build
```

### Flashing
```bash
idf.py -p <PORT> flash
```
