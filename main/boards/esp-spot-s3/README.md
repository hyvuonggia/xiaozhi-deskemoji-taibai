# ESP-Spot S3

## Introduction

<div align="center">
    <a href="https://oshwhub.com/esp-college/esp-spot"><b> LCSC Open-Source Platform </b></a>
    |
    <a href="https://www.bilibili.com/video/BV1ekRAYVEZ1/"><b> Bilibili Demo </b></a>
</div>

ESP-Spot is a smart voice-interaction box open-sourced by ESP Friends. It includes a microphone, speaker, and IMU inertial sensor, and can run on battery power. ESP-Spot has no display, but includes an RGB indicator and two buttons. See the [LCSC open-source project](https://oshwhub.com/esp-college/esp-spot) for hardware details.

The ESP-Spot open-source project uses an ESP32-S3-WROOM-1-N16R8 module. If you use a different flash size when building your own version, update the corresponding settings.


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

- `Xiaozhi Assistant` → `Board Type` → select `ESP-Spot-S3`

Press `S` to save and `Q` to exit.

**Build**

```bash
idf.py build
```

**Flash**

```bash
idf.py flash
```

> [!TIP]
>
> **If the computer cannot detect the ESP-Spot serial port, try the following:**
> 1. Open the front cover.
> 2. Remove the PCB with the module.
> 3. Hold <kbd>BOOT</kbd> while reinserting the PCB, taking care not to reverse its orientation.
> 
> ESP-Spot should now be in download mode. After flashing, you may need to disconnect and reconnect the PCB.
