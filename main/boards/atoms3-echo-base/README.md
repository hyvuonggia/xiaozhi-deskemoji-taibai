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
Xiaozhi Assistant -> Board Type -> AtomS3 + Echo Base
```

**Disable voice wake-up:**

```
Xiaozhi Assistant -> [ ] Enable voice wake-up and audio processing -> Unselect
```

**Change the flash size:**

```
Serial flasher config -> Flash size -> 8 MB
```

**Change the partition table:**

```
Partition Table -> Custom partition CSV file -> partitions/v1/8m.csv
```

**Disable external PSRAM:**

```
Component config -> ESP PSRAM -> [ ] Support for external, SPI-connected RAM -> Unselect
```

**Build:**

```bash
idf.py build
```