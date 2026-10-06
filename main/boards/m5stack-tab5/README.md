# Instructions

* [M5Stack Tab5 docs](https://docs.m5stack.com/zh_CN/core/Tab5)

## Quick Start

Download the prebuilt [firmware](https://pan.baidu.com/s/1dgbUQtMyVLSCSBJLHARpwQ?pwd=1234) Extraction code: 1234

```shell
esptool.py --chip esp32p4 -p /dev/ttyACM0 -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x00 tab5_xiaozhi_v1_addr0.bin 
```

## Basic Usage

* idf version: v5.5-dev

1. Set the build target to esp32p4

```shell
idf.py set-target esp32p4 
```

2. Update the configuration

```shell
cp main/boards/m5stack-tab5/sdkconfig.tab5 sdkconfig
```

3. Build and flash the firmware

```shell
idf.py build flash monitor
```

> [!NOTE]
> To enter download mode, press and hold the reset button (about 2 seconds) until the internal green LED starts flashing rapidly, then release the button.


## log

@2025/05/17 Test issues

1. listening... Does it take a few seconds to receive voice input?
2. Brightness adjustment is incorrect
3. Volume adjustment is incorrect
 
## TODO
