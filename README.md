# CH32 Meteostation

This project is a meteostation using the CH32V003A4M6 microcontroller. It includes functionalities for measuring temperature, humidity, and pressure, and displays the data on an SSD1306 OLED screen. The project also supports Bluetooth communication for remote data access.

## Features

- Temperature and humidity measurement using AHT20 sensor
- Pressure measurement using BMP280 sensor
- Display data on SSD1306 OLED screen
- Bluetooth communication using JDY-23 module
- Battery voltage monitoring
- Deep sleep mode for power saving

## Hardware Requirements

- CH32 microcontroller
- AHT20 temperature and humidity sensor
- BMP280 pressure sensor
- SSD1306 OLED display
- JDY-23 Bluetooth module
- Battery and power management components

## Software Requirements

- PlatformIO
- CH32V003 SDK
- I2C, UART, and GPIO libraries

## Setup

1. Clone the repository:
    ```sh
    git clone https://github.com/yourusername/CH32Meteostation.git
    cd CH32Meteostation
    ```

2. Open the project in PlatformIO:
    ```sh
    platformio init --board ch32v003
    ```

3. Build and upload the firmware:
    ```sh
    platformio run --target upload
    ```

## Usage

1. Power on the device.
2. The device will initialize and start measuring the environmental data.
3. The data will be displayed on the OLED screen.
4. Use a Bluetooth-enabled device to connect and retrieve data remotely.

## Code Overview

- `main.c`: The main application code, including initialization, measurement, display, and communication functions.
- `i2c.c` and `i2c.h`: I2C communication functions.
- `uart.c` and `uart.h`: UART communication functions.
- `ssd1306.c` and `ssd1306.h`: SSD1306 OLED display functions.
- `bmp280.c` and `bmp280.h`: BMP280 sensor functions.
- `aht20.c` and `aht20.h`: AHT20 sensor functions.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Acknowledgements

- [PlatformIO](https://platformio.org/)
- [ch32fun](https://github.com/cnlohr/ch32fun)
