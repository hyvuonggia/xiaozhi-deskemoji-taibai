# DFRobot UNIHIKER K10

## Button Configuration
* A: Short press — interrupt/wake up; hold for 1 s — increase volume
* B: Short press — interrupt/wake up; hold for 1 s — decrease volume

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
Xiaozhi Assistant -> Board Type -> DFRobot Unihiker K10
```

**Change the PSRAM configuration:**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**Build:**

```bash
idf.py build
```