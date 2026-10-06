# Because the original microphone model has been discontinued, Taiji Pi (JC3636W518) units manufactured after July 2025 use a different microphone and screen glass. For devices with a batch number greater than 2528 on the product label, select I2S Type PDM.

# Build configuration commands

**Set the build target to ESP32S3：**

```bash
idf.py set-target esp32s3
```

**Open menuconfig：**

```bash
idf.py menuconfig
```

**Select board：**

```
Xiaozhi Assistant -> Board Type -> `太极小派esp32s3` (Taiji Pi ESP32-S3)

Xiaozhi Assistant -> taiji-pi-S3 I2S Type -> I2S Type PDM
```

**Change the PSRAM configuration：**

```
component config -> ESP PSRAM -> SPI RAM config -> Try to allocate memories of WiFi and LWIP in SPIRAM firstly. If failed, allocate internal memory

```

**Build：**

```bash
idf.py build
```
