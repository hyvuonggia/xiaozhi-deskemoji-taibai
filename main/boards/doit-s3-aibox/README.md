# DOIT Smart Companion Box

# Features
* Uses a PDM microphone
* Uses a common-anode LED

## Button Configuration
* BUTTON3: Short press — interrupt/wake up
* BUTTON1: Volume up
* BUTTON2: Volume down

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
Xiaozhi Assistant -> Board Type -> `四博智联AI陪伴盒子` (Doit AI companion box)
```

**Change the PSRAM configuration:**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**Build:**

```bash
idf.py build
```