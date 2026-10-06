# DFRobot ESP32-S3 AI Camera Module

## Introduction
ESP32-S3 AI CAM is an intelligent camera module based on the ESP32-S3 chip, designed for video/image processing and voice interaction. It is suitable for AI projects such as video surveillance, edge image recognition, and voice conversations.
![](https://ws.dfrobot.com.cn/FsTrGbrX2NZAwzWS8OSQGOGikuYA)

[Click for a detailed overview](https://wiki.dfrobot.com.cn/SKU_DFR1154_ESP32_S3_AI_CAM)

[Click to view the vision feature demo](https://www.bilibili.com/video/BV1ktjSzNEUU/)

# Features
* Uses a PDM microphone
* Onboard OV3660 camera

## Button Configuration
* BOOT: Short press — interrupt/wake up

## Build Configuration Commands

**Set the build target to ESP32S3:**

```bash
idf.py set-target esp32s3
```

**Open menuconfig:**

```bash
idf.py menuconfig
```

**Select the board:**

```
Xiaozhi Assistant -> Board Type -> `DFRobot ESP32-S3 AI智能摄像头模块` (DFRobot ESP32-S3 AI camera module)
```

**Change the PSRAM configuration:**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**Set the Wi-Fi transmission power to 10:**

```
Component config -> PHY -> (10)Max WiFi TX power (dBm)
```

**Build:**

```bash
idf.py build
```