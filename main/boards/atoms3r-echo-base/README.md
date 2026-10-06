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
Xiaozhi Assistant -> Board Type -> AtomS3R + Echo Base
```

**Change the flash size:**

```
Serial flasher config -> Flash size -> 8 MB
```

**Change the partition table:**

```
Partition Table -> Custom partition CSV file -> partitions/v1/8m.csv
```

**Change the PSRAM configuration:**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**Build:**

```bash
idf.py build
```