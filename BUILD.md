# ESP32 Marauder — Build Setup for ESP32-C5 (Pwnterrey)

## Prerequisites

- [Arduino CLI](https://arduino.github.io/arduino-cli/) or Arduino IDE
- ESP32 Arduino Core **3.3.4**

## Board Configuration

| Option | Value |
|---|---|
| Board | `esp32:esp32:esp32c5` (ESP32C5 Dev Module) |
| Flash Size | 4MB |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |
| USB CDC On Boot | Enabled (cdc) |
| PSRAM | Disabled |
| CPU Frequency | 240MHz |
| Flash Mode | QIO |
| Flash Frequency | 80MHz |

### FQBN (arduino-cli)

```
esp32:esp32:esp32c5:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=huge_app,PSRAM=disabled
```

## Libraries

### Included in ESP32 Core 3.3.4

| Library | Version |
|---|---|
| FS | 3.3.4 |
| SPIFFS | 3.3.4 |
| WiFi | 3.3.4 |
| Networking | 3.3.4 |
| DNSServer | 3.3.4 |
| ESP32 Async UDP | 3.3.4 |
| SD | 3.3.4 |
| SPI | 3.3.4 |
| Update | 3.3.4 |
| Wire | 3.3.4 |
| Hash | 3.3.4 |

### External (install manually)

| Library | Version |
|---|---|
| LinkedList | 1.3.3 |
| MicroNMEA | 2.0.6 |
| EspSoftwareSerial | 8.1.0 |
| ArduinoJson | 7.4.2 |
| NimBLE-Arduino | 2.3.8 |
| ESP32Ping | 1.7 |
| ESP Async WebServer | 3.8.1 |
| Async TCP | 3.4.8 |
| Adafruit NeoPixel | 1.15.4 |
| Adafruit MAX1704X | 1.0.2 |
| Adafruit BusIO | 1.14.1 |

> **Note:** ESP32Ping is not available in the Arduino Library Manager. Install it manually from [GitHub](https://github.com/marian-craciunescu/ESP32Ping).

## Build with arduino-cli

```bash
# Install ESP32 core
arduino-cli core install esp32:esp32@3.3.4

# Install libraries
arduino-cli lib install "LinkedList@1.3.3" "MicroNMEA@2.0.6" "EspSoftwareSerial@8.1.0" \
  "ArduinoJson@7.4.2" "NimBLE-Arduino@2.3.8" "Adafruit NeoPixel@1.15.4" \
  "Adafruit MAX1704X@1.0.2" "Adafruit BusIO@1.14.1" \
  "ESP Async WebServer@3.8.1" "Async TCP@3.4.8"

# Compile
arduino-cli compile \
  --fqbn "esp32:esp32:esp32c5:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=huge_app,PSRAM=disabled" \
  esp32_marauder/esp32_marauder.ino

# Flash
arduino-cli upload \
  --fqbn "esp32:esp32:esp32c5:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=huge_app,PSRAM=disabled" \
  --port /dev/ttyACM0 \
  esp32_marauder/esp32_marauder.ino
```

## Flash with esptool (manual)

```bash
esptool --chip esp32c5 --port /dev/ttyACM0 --baud 921600 write-flash \
  0x2000 bootloader.bin \
  0x8000 partitions.bin \
  0xe000 boot_app0.bin \
  0x10000 esp32_marauder.ino.bin
```

> **Important:** The ESP32-C5 bootloader offset is `0x2000`, not `0x0`.

## Board Target in configs.h

Make sure `MARAUDER_C5` is the only active board define:

```cpp
#define MARAUDER_C5
```
