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
Xiaozhi Assistant -> Board Type -> LILYGO T-CameraPlus-S3_V1_0_V1_1 or LILYGO T-CameraPlus-S3_V1_2
```

**Change the PSRAM configuration：**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Quad Mode PSRAM
```

**Build：**

```bash
idf.py build
```

<a href="https://github.com/Xinyuan-LilyGO/T-CameraPlus-S3" target="_blank" title="LILYGO T-CameraPlus-S3">LILYGO T-CameraPlus-S3</a>