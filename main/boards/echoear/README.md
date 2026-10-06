# EchoEar

## Introduction

<div align="center">
    <a href="https://oshwhub.com/esp-college/echoear"><b> LCSC Open-Source Platform </b></a>
</div>

EchoEar is an intelligent AI development kit featuring an ESP32-S3-WROOM-1 module, a 1.85-inch QSPI round touchscreen, a dual-microphone array, offline voice wake-up, and sound-source localization. See the [LCSC open-source project](https://oshwhub.com/esp-college/echoear) for hardware details.

## Build Configuration Commands

**Set the build target to ESP32S3**

```bash
idf.py set-target esp32s3
```

**Open and configure menuconfig**

```bash
idf.py menuconfig
```

Configure the following options:

### Basic Configuration
- `Xiaozhi Assistant` → `Board Type` → select `EchoEar`

### Partition Table Configuration
- `Partition Table` → `Partition Table` → select `Custom partition table CSV`
- `Partition Table` → `Custom partition CSV file` → enter `partitions/v1/16m_echoear.csv`

### UI Style Selection

EchoEar supports two different UI display styles, selected by changing a macro definition in the code:

#### Custom Emote Display System (Recommended)
```c
#define USE_LVGL_DEFAULT    0
```
- **Features**: Uses the custom `EmoteDisplay` emote display system
- **Functions**: Supports rich emote animations, eye animations, and status icon display
- **Use case**: Smart assistant scenarios, for more expressive human-computer interaction
- **Classes**: `anim::EmoteDisplay` + `anim::EmoteEngine`

#### Default LVGL Display System
```c
#define USE_LVGL_DEFAULT    1
```
- **Features**: Uses the standard LVGL graphics library display system
- **Functions**: Traditional text-and-icon interface
- **Use case**: Applications that need standard GUI controls
- **Class**: `SpiLcdDisplay`

#### How to Change the Setting
1. Open `main/boards/echoear/EchoEar.cc`
2. Find the macro definition on line 29: `#define USE_LVGL_DEFAULT    0`
3. Change it to the desired value (0 or 1)
4. Rebuild the project

> **Note**: EchoEar uses 16 MB of flash and requires a dedicated partition table to allocate storage appropriately for the application, OTA updates, resource files, and other data.

Press `S` to save and `Q` to exit.

**Build**

```bash
idf.py build
```

**Flash**

Connect EchoEar to the computer, **make sure it is powered on**, and run:

```bash
idf.py flash
```