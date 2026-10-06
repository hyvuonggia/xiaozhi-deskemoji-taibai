
The minsi-k08-wifi and minsi-k08-ml307 are Minsi Technology solutions based on the ESP32S3N16R8, equipped with a MAX98357 audio power amplifier and INMP441 omnidirectional microphone module. They are created by modifying the K08 transparent Mecha Mini-Cannon speaker into a punk-style Xiaozhi AI chatbot with a large speaker and battery.

<a href="https://item.taobao.com/item.htm?id=889892765588" target="_blank" title="SenseCAP Watcher">Minsi-k08</a>

  <a href="minsi-k08.jpg" target="_blank" title="Minsi-k08">
    <img src="minsi-k08.jpg" width="240" />
  </a>



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
Xiaozhi Assistant -> Board Type -> `敏思科技K08(DUAL)` (Minsi Technology K08 Dual)
```

**Build and flash：**

```bash
idf.py build flash
```