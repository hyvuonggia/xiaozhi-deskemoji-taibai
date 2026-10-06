# Instructions


1. Set the build target to esp32s3

```shell
idf.py set-target esp32s3
```

2. Update the configuration

```shell
cp main/boards/m5stack-core-s3/sdkconfig.cores3 sdkconfig
```

3. Build and flash the firmware

```shell
idf.py build flash monitor
```

> [!NOTE]
> To enter download mode, press and hold the reset button (about 3 seconds) until the internal indicator turns green, then release the button.


 
