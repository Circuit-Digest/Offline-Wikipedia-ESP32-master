# Offline Wikipedia on ESP32-S3

Run a complete, searchable copy of Wikipedia on a low-cost **ESP32-S3** board, with **no internet connection required**. The ESP32 hosts its own Wi-Fi access point and web server, serving articles (and images) straight from a microSD card. Connect with any phone, tablet, or laptop and browse.

This is made possible by [Kiwix](https://www.kiwix.org/), a non-profit that makes free knowledge available offline through open-source ZIM archives.

> Original tutorial: [Circuit Digest - How to Build an Offline Wikipedia Using ESP32](https://circuitdigest.com/)
> Repository: https://github.com/Circuit-Digest/Offline-Wikipedia-ESP32-master

---

## Features

- Fully offline: no cloud, no router, no internet
- Built-in Wi-Fi access point (`Wikipedia-Offline`), open network
- Search bar with live article suggestions
- Full article text, with optional images and diagrams
- Works with any language or topic Kiwix provides (Wikipedia, physics, mathematics, chess, football, etc.)
- Serial Monitor remains active for diagnostics

---

## Hardware Required

| Component | Notes |
|-----------|-------|
| ESP32-S3 board with microSD slot | e.g. ESP32-S3 CAM module. **ESP32-S3 is required as the firmware expects an ESP32-S3 board** |
| microSD card | 16 GB or 32 GB recommended. A 2-4 GB card works if the data fits |
| USB data cable | For flashing the firmware |
| PC with VS Code + PlatformIO | Used for preprocessing and flashing |

---

## Step-by-Step Setup

### Step 1: Download the correct ZIM file

Go to the Kiwix Wikipedia dump page and pick a file whose size fits your microSD card.

File names follow a pattern such as `wikipedia_en-simple_all_maxi_2026-09.zim`:

| Tag | Meaning |
|-----|---------|
| `en`, `af`, `ru`, `fr`, ... | Language code (English, Afrikaans, Russian, French, etc.) |
| `simple` | Simple English Wikipedia: easier vocabulary, fewer articles, much smaller file |
| `all` | General articles across all topics, not a single subject |
| `maxi` | Full version with complete text and images |
| `nopic` | Text only, no pictures, much smaller |
| `mini` | Lead/intro paragraphs of articles only |
| `2026-XX` | Year and month the snapshot was taken |

Topic-specific archives (physics, mathematics, football, chess, ...) are also available. If you don't want images, choose a `nopic` file.

### Step 2: Clone the repository

```bash
git clone https://github.com/Circuit-Digest/Offline-Wikipedia-ESP32-master.git
cd Offline-Wikipedia-ESP32-master
```

All following commands are run from the root of the cloned repository (the tutorial uses VS Code with PlatformIO).

### Step 3: Preprocess the ZIM file

This re-compresses the ZIM contents so the ESP32 can read them efficiently. It extracts all articles, renders images/diagrams into `.dat` chunks, and generates the index files.

The default thumbnail size is `320x212`. The example below uses `480x320` for sharper images.

```powershell
& "C:\Users\<YourUser>\.platformio\penv\Scripts\python.exe" preprocessor\build_wiki_db.py --all-jpeg --thumb-size 480x320 "C:\Users\<YourUser>\Downloads\wikipedia_en-simple_all_maxi_2026-09.zim" preprocessor\output_en_wiki
```

- Replace `<YourUser>` and the `.zim` path with your own.
- Processing can take a while, depending on the ZIM size.
- The output folder (`output_en_wiki`) can be renamed to match your language, e.g. `output_af_wiki` for Afrikaans.

### Step 4: Copy the data to the microSD card

1. Plug the microSD card into your PC and **format it as FAT32** (Windows: right-click the drive > Format > FAT32).
2. Create a folder named `wiki` in the root of the card.
3. Copy **all files** from `preprocessor\output_en_wiki\` into `/wiki/`.

The card should look like this:

```
SD Card Root/
└── wiki/
    ├── index.bin
    ├── sparse_index.bin
    ├── id_index.bin
    ├── index_meta.txt
    ├── articles_0000.dat
    ├── articles_0001.dat
    ...
    ├── img_index.bin
    ├── img_meta.txt
    ├── img_0000.dat
    ├── img_0001.dat
    ...
```

Insert the card into the microSD slot on the ESP32-S3 board.

### Step 5: Flash the web server firmware

Connect the ESP32-S3 over USB and run:

```powershell
cd firmware
$env:PYTHONIOENCODING="utf-8"
& "C:\Users\<YourUser>\.platformio\penv\Scripts\pio.exe" run -e s3-cam-web -t upload
```

No changes are needed. A success message appears when flashing completes. If it fails, check that you are using an **ESP32-S3** board, as the firmware expects one.

### Step 6: Verify the boot

Open the Serial Monitor:

```powershell
& "C:\Users\<YourUser>\.platformio\penv\Scripts\pio.exe" device monitor
```

A healthy boot looks like this (article counts vary by ZIM file):

```
[wiki_db] Initialising storage...
[wiki_db] Setting custom SD_MMC pins: CLK=39, CMD=38, D0=40
[wiki_db] SD_MMC mounted successfully (1-bit mode)!
[wiki_db] Database ready: 129909 articles
[wiki_db] Images: 129909 available
```

### Step 7: Connect and browse

1. Power on the ESP32-S3.
2. On your phone, tablet, or laptop, connect to the Wi-Fi network **`Wikipedia-Offline`** (open network, no password).
3. Open a browser and go to **http://192.168.4.1/**

You'll see the "Welcome to Offline Wikipedia" page with the total article count. Start typing in the search bar to see matching articles, then click one to read the full content (text and images, or text only if you used a `nopic` file).

---

## Troubleshooting

| Problem | Likely cause |
|---------|--------------|
| Firmware upload fails | Board is not an ESP32-S3 |
| SD card not mounted in Serial Monitor | Card not formatted as FAT32, or files not inside `/wiki/` |
| Not enough space on card | ZIM output is larger than the card; use `simple`, `nopic`, or `mini`, or a bigger card |
| Can't open the page | Confirm you're connected to `Wikipedia-Offline` and use `http://192.168.4.1/` |

---

## Ideas for Extension

- Add a **TFT display** to read articles directly on the device instead of through a browser
- Integrate an **offline text-to-speech** system so articles are read aloud
- Build an emergency survival kit or off-grid educational tool

---

## Credits

- [Kiwix](https://www.kiwix.org/) for the open ZIM archives
- [Circuit Digest](https://circuitdigest.com/) for the original tutorial and code

## License

Check the repository's license file for details. Wikipedia content is available under its own open licenses (CC BY-SA).
