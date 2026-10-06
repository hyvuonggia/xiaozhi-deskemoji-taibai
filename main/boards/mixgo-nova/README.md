# Mixgo_Nova Development Board

<img src="https://mixly.cn/public/icon/2024/6/09705006c1c643beb96338791ee1dea0_m.png" alt="Mixgo_Nova" width="200"/>

&zwnj;**[Mixgo_Nova](https://mixly.cn/fredqian/mixgo_nova)**&zwnj; is a multifunctional development board designed for IoT, education, and maker projects. It integrates a range of sensors and wireless communication modules, supports visual programming (Mixly) and offline voice interaction, and is suitable for rapid prototyping and teaching.

---

## 🛠️  Build configuration commands

**ES8374 CODE MICcapture issue：**

```
managed_components\espressif__esp_codec_dev\device\es8374

static int es8374_config_adc_input(audio_codec_es8374_t *codec, es_adc_input_t input)
{
    int ret = 0;
    int reg = 0;
    ret |= es8374_read_reg(codec, 0x21, &reg);
    if (ret == 0) {
        reg = (reg & 0xcf) | 0x24;
        ret |= es8374_write_reg(codec, 0x21, reg);
    }
    return ret;
}

PS: L386 reg = (reg & 0xcf) | 0x14; change to reg = (reg & 0xcf) | 0x24;
```

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
Xiaozhi Assistant -> Board Type -> `元控·青春` (Mixgo Nova Youth Edition)
```

**Change the PSRAM configuration：**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> QUAD Mode PSRAM
```

**Change the Flash configuration：**

```
Serial flasher config -> Flash size -> 8 MB
Partition Table -> Custom partition CSV file -> partitions/v1/8m.csv
```

**Build：**

```bash
idf.py build
```

**Merge BIN：**

```bash
idf.py merge-bin -o xiaozhi-nova.bin -f raw
```