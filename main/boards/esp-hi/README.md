# ESP-Hi

## Introduction

<div align="center">
    <a href="https://oshwhub.com/esp-college/esp-hi"><b> LCSC Open-Source Platform </b></a>
    |
    <a href="https://www.bilibili.com/video/BV1BHJtz6E2S"><b> Bilibili </b></a>
</div>

ESP-Hi is an ultra-**low-cost** AI conversational robot based on ESP32-C3 and open-sourced by ESP Friends. It integrates a 0.96-inch color display for showing expressions, and the **robot dog supports dozens of movements**. By making extensive use of ESP32-C3 peripherals, it provides audio input and output with minimal board-level hardware. The software is also optimized to reduce RAM and flash usage, enabling both **wake-word detection** and multiple peripheral drivers on a resource-constrained device. See the [LCSC open-source project](https://oshwhub.com/esp-college/esp-hi) for hardware details.

## WebUI

ESP-Hi x Xiaozhi includes a Web UI for controlling body movements. Connect your phone and ESP-Hi to the same Wi-Fi network, then visit `http://esp-hi.local/` on your phone.

To disable it, unset `ESP_HI_WEB_CONTROL_ENABLED`, i.e. uncheck `Component config` → `Servo Dog Configuration` → `Web Control` → `Enable ESP-HI Web Control`.

## Build Configuration Commands

ESP-Hi requires many sdkconfig options, so using the build script is recommended.

**Build**

```bash
python ./scripts/release.py esp-hi
```

For a manual build, use `esp-hi/config.json` as a reference when changing the corresponding menuconfig options.

**Flash**

```bash
idf.py flash
```


> [!TIP]
>
> **Servo control uses ESP-Hi’s USB Type-C interface**, preventing connection to a computer (and therefore flashing or viewing runtime logs). If this happens, follow these steps:
>
> **Flash**
>
> 1. Disconnect ESP-Hi from power. Keep only the head; do not connect the body.
> 2. Hold down the ESP-Hi button while connecting it to the computer.
> 
> ESP-Hi (ESP32C3) should now be in flashing mode, and you can flash the firmware from the computer. You may need to disconnect and reconnect the power after flashing.
>
> **View the log**
>
> Set `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`; in other words, select `USB Serial/JTAG Controller` under `Component config` → `ESP System Settings` → `Channel for console output`. This also disables servo control.
