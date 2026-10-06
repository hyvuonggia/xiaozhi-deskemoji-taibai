# Product information

```http
https://e.tb.cn/h.6Gl2LC7rsrswQZp?tk=qFuaV9hzh0k CZ356
```

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
Xiaozhi Assistant -> Board Type -> zhengchen-1.54tft-wifi
```

```

**Build：**

bash
idf.py build
```

**Flash:**
idf.py build flash monitor

Flash the firmware and display the log


**Generate firmware:**

```bash
idf.py merge-bin
```
