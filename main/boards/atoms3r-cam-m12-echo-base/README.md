# AtomS3R CAM/M12 + Echo Base

## Introduction

<div align="center">
    <a href="https://docs.m5stack.com/zh_CN/core/AtomS3R%20Cam"><b> AtomS3R CAM Product Page </b></a>
    |
    <a href="https://docs.m5stack.com/zh_CN/core/AtomS3R-M12"><b> AtomS3R M12 Product Page </b></a>
    |
    <a href="https://docs.m5stack.com/zh_CN/atom/Atomic%20Echo%20Base"><b> Echo Base Product Page </b></a>
</div>

AtomS3R CAM and AtomS3R M12 are IoT-programmable controllers from M5Stack based on the ESP32-S3-PICO-1-N8R8 and equipped with cameras. Atomic Echo Base is a voice-recognition base designed for M5 Atom series hosts, integrating an ES8311 mono audio codec, MEMS microphone, and NS4150B power amplifier.

Neither development board has a screen or extra buttons, so voice wake-up is required. If needed, use `idf.py monitor` to check the log and determine the device status.

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

- `Xiaozhi Assistant` → `Board Type` → select `AtomS3R CAM/M12 + Echo Base`
- `Xiaozhi Assistant` → `IoT Protocol` → select `MCP Protocol` to enable camera recognition
- `Partition Table` → `Custom partition CSV file` → delete the existing content and enter `partitions/v1/8m.csv`
- `Serial flasher config` → `Flash size` → select `8 MB`

Press `S` to save and `Q` to exit.

**Build**

```bash
idf.py build
```

**Flash**

Connect the AtomS3R CAM/M12 to the computer and hold the side RESET button until the green light below it flashes.

```bash
idf.py flash
```

After flashing, press RESET once to restart.
