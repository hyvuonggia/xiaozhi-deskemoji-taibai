# AtomEchoS3R
## Introduction

AtomEchoS3R is an IoT-programmable controller from M5Stack based on the ESP32-S3-PICO-1-N8R8, integrating an ES8311 mono audio codec, MEMS microphone, and NS4150B power amplifier.

The development board has **no display or extra buttons**, so voice wake-up is required. If needed, use `idf.py monitor` to check the log and determine the device status.

## Build Configuration Commands

**Set the build target to ESP32S3**

```bash
idf.py set-target esp32s3
```

**Open and configure menuconfig**

```bash
idf.py menuconfig
```

Configure the following options:

- `Xiaozhi Assistant` → `Board Type` → select `AtomEchoS3R`
- `Partition Table` → `Custom partition CSV file` → delete the existing content and enter `partitions/v1/8m.csv`
- `Serial flasher config` → `Flash size` → select `8 MB`
- `Component config` → `ESP PSRAM` → `Support for external, SPI-connected RAM` → `SPI RAM config` → select `Octal Mode PSRAM`

Press `S` to save and `Q` to exit.

**Build**

```bash
idf.py build
```

**Flash**

Connect the AtomEchoS3R to the computer and hold the side RESET button until the green light below it flashes.

```bash
idf.py flash
```

After flashing, press RESET once to restart the device.
