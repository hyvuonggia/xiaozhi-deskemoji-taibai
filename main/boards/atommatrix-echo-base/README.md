# Build Configuration Commands

**Set the build target to ESP32:**

```bash
idf.py set-target esp32
```

**Open menuconfig:**

```bash
idf.py menuconfig
```

**Select the board:**

```
Xiaozhi Assistant -> Board Type -> AtomMatrix + Echo Base
```

**Change the flash size:**

```
Serial flasher config -> Flash size -> 4 MB
```

**Change the partition table:**

```
Partition Table -> Custom partition CSV file -> partitions/v1/4m.csv
```

**Build:**

```bash
idf.py build
```