# labplus Ledong V2

## Onboard Resources
    Main controller：ESP32-S3 external8MB psram 16MB flash
    Sensors:
        Buttons(A Bbutton）	IO0 IO46
        Light sensor	IIC
        6-axis sensor	IIC
        Magnetometer	IIC
        Sound trigger	IO6
        Touch buttons IIC P Y T H O N
        Camera	IIC
    Actuators:
        Buzzer	IO21
        RGB LED	IO16
        Audio recording/playback es8388	IIC
        TFT LCD	jd9853 SPI
        Motor driver	IIC


## Build Configuration

### Set the build target to ESP32S3 and use USB JTAG for downloading

```bash
idf.py set-target esp32s3
```

### menuconfig configuration

```bash
idf.py menuconfig
```

***Select board：***

```
Xiaozhi Assistant -> Board Type -> labplus Ledong_v2 board
```

***Change the PSRAM configuration：***

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> quad Mode PSRAM
```

**Build：**

```bash
idf.py build
```

**Package firmware：**

```bash
esptool.py -p /dev/ttyACM0 -b 1500000 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 bootloader/bootloader.bin 0x100000 xiaozhi.bin 0x8000 partition_table/partition-table.bin 0xd000 ota_data_initial.bin 0x10000 srmodels/srmodels.bin 
```

## Usage

### Button configuration
* A：Short press — interrupt/wake
