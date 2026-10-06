# jiuchuan-xiaozhi-sound
Jiuchuan Technology Xiaozhi AI Speaker

## 🛠️ Build Guide
**Development environment**：ESP-IDF v5.4.1

### Build steps：
> ⚠️ **Note**：If accessing online libraries fails during compilation, try changing the accelerator setting or editing [idf_component.yml] to use a domestic mirrors.

1. Open the project folder in VSCode;
2. Clean the project（Clean Project）；
3. Set the ESP-IDF version to `v5.4.1`；
4. Click the prompt in the bottom-right corner of VSCode to generate [compile_commands.json];
5. Set the target device to `[esp32s3] -> [JTAG]`；
6. Open **SDK Configuration Editor**；
7. Set the custom partition table path to：`partitions/v1/16m.csv`；
8. Set **Board Type** to **Jiuchuan Technology** (the original menu label is in Chinese);
9. Save the configuration and start building.

## 🔌 Flashing Steps
1. Connect the computer and speaker with a USB cable;
2. With the device powered off, press and hold the power button;
3. In the flashing tool, select the corresponding serial port (COM Port);
4. Click Flash and select UART mode;
5. Do not release the power button until flashing is complete.