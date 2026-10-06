[Product Overview]
[] ESP32-S3 Muma rocking-horse development board, 1.54-inch LCD, Xiaozhi/Muma/Xiage AI DeepSeek voice chatbot, N16R8
[Features]
[] Cute rocking horse; supports weather clock, SD video playback, and AI conversations. All firmware source code is open source, suitable for children learning to program and for developing additional features.
Xiaozhi AI supports voice wake-up. The touch version also supports touch wake-up and interruption.
Display:1.54-inchST7789 240x240resolution
Product link:
https://spotpear.cn/shop/ESP32-S3-AI-1.54-inch-LCD-Display-TouchScreen-N16R8-muma-DeepSeek/sp-esp32-s3-1.54-muma-W-Bat.html

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
Xiaozhi Assistant -> Board Type -> Spotpear ESP32-S3-LCD-1.54-MUMA
```

**Build：**

```bash
idf.py build
```
