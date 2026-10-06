Hardware is based on the ESP32S3CAM development board; the code is adapted from bread-compact-wifi-lcd.
The camera used is an OV2640.
Note: The camera uses many I/O pins, so it occupies ESP32S3 USB pins 19 and 20.
Refer to the pin definitions in config.h for the wiring.

 
# Build Configuration Commands

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
Xiaozhi Assistant -> Board Type -> `面包板新版接线（WiFi）+ LCD + Camera` (Breadboard wiring v2, Wi-Fi + LCD + camera)
```

**Build and flash:**

```bash
idf.py build flash
```