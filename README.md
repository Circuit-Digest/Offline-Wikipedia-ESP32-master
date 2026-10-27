# ESP32 Offline Wikipedia Server 🌐 SD Card Database with Kiwix & LZ4

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Supported-orange.svg)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-blue.svg)](https://www.arduino.cc/)

Host a fully functional, offline instance of Wikipedia on an **ESP32**, **ESP32-S3**, or **ESP32-CAM** microcontroller using compressed Kiwix `.zim` data. 

By leveraging custom LZ4 decompression, optimized binary search indexes, and lightweight JPEG thumbnails, this project allows you to serve millions of Wikipedia articles directly from a microSD card over a local Wi-Fi Access Point—**zero internet connection required.**

---

## 📌 Features

* **100% Offline Access:** Creates its own local Wi-Fi Access Point (`192.168.4.1`) to serve web pages to any smartphone, tablet, or laptop.
* **On-the-Fly LZ4 Decompression:** Fast, real-time article un-chunking directly in PSRAM.
* **Optimized Image Support:** Custom Python preprocessor resizes and encodes thumbnails ($480 \times 320$) for fast rendering on microcontroller hardware.
* **Ultra-Fast Binary Search:** Native indexing allows instant article retrieval without scanning the entire SD card.
* **Broad Hardware Support:** Configurable for standard ESP32 boards (WROVER), ESP32-S3 variants, and cheap ESP32-CAM modules.

---

## 🛠️ Hardware Requirements

| Component | Recommendation / Requirement | Notes |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32-S3, ESP32-WROVER, or ESP32-CAM | **PSRAM is mandatory.** Standard WROOM-32 without PSRAM is not supported. |
| **PSRAM** | 2 MB Minimum, **8 MB Recommended** | Required for LZ4 decompression buffers and HTTP dynamic allocations. |
| **Storage** | microSD Card (Class 10 / FAT32) | Size depends on the `.zim` dump used (16 GB - 64 GB recommended). |
| **SD Adapter** | SPI SD Module or Built-in SDMMC | Integrated slot on ESP32-CAM or ESP32-S3 DevKit. |

---

## 💻 Software Prerequisites

* [PlatformIO IDE](https://platformio.org/) (VS Code Extension)
* [Python 3.8+](https://www.python.org/) (For running the database preprocessor script)
* A Kiwix `.zim` archive file (e.g., Simple English Wikipedia from [Kiwix Downloads](https://download.kiwix.org/zim/wikipedia/))

---

## 🚀 Quick Start Guide

### Step 1: Preprocess the Kiwix `.zim` File

1. Download your desired `.zim` file from Kiwix.
2. Run the included Python preprocessor script to convert, resize images, and generate the LZ4 index database structure for the ESP32:

```bash
python preprocessor/build_wiki_db.py --all-jpeg --thumb-size 480x320 "path/to/wikipedia.zim" preprocessor/output_wiki
```

3. Copy the output folder structure generated in `preprocessor/output_wiki` directly onto the root directory of your FAT32-formatted microSD card.
4. Insert the microSD card into your ESP32's SD slot.

---

### Step 2: Configure and Flash Hardware

1. Clone this repository:
   ```bash
   git clone https://github.com/your-username/ESP32-Offline-Wikipedia.git
   cd ESP32-Offline-Wikipedia
   ```

2. Open the project in **PlatformIO**.

3. Select your target environment in `platformio.ini` (e.g., `esp32-s3-devkitc-1` or `esp32cam`).

4. Verify or adjust the SD card pin mappings in `include/config.h`:

```cpp
// Example: Standard ESP32 SPI Pinout
#define SD_CS   5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK  18
```

5. Build and upload the code to your ESP32 board.

---

## 📱 How to Use

1. Power on your ESP32 board.
2. On your phone, laptop, or computer, search for the Wi-Fi Access Point named **`ESP32-Wikipedia`**.
3. Connect to the network (no internet warning is normal).
4. Open your web browser and navigate to:
   ```text
   http://192.168.4.1
   ```
5. Search for any article and enjoy your pocket-sized off-grid encyclopedia!

---

## ⚙️ Board-Specific Configuration Notes

### ESP32-S3 Setup
Ensure Octal/Quad PSRAM flags are properly matched in your `platformio.ini`:
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi
build_flags = 
    -D BOARD_HAS_PSRAM
    -D CONFIG_ESP32S3_SPIRAM_SUPPORT
```

### ESP32-CAM Setup
The ESP32-CAM uses the onboard **SDMMC interface** instead of standard SPI:
* Use `#include "SD_MMC.h"` in your code instead of `SD.h`.
* Connect **GPIO 0 to GND** while flashing, then disconnect and press RESET.
* Standard pin configuration uses 1-bit or 4-bit bus mode (Note: GPIO 4 is shared with the white flash LED).

---

## 🤝 Contributing

Contributions, bug reports, and optimizations are welcome! Feel free to open an issue or submit a pull request.

---

## 📜 License & Acknowledgments

* Distributed under the **MIT License**.
* Special thanks to the **Kiwix Project** for making open knowledge accessible worldwide.
* Powered by [LZ4 Compression](https://github.com/lz4/lz4).
