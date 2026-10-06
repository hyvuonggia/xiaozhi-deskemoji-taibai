# ESP32 UNO D1 R32 Development Board - Xiaozhi AI Desktop Robot

This directory contains the configuration files and implementation code for building a Xiaozhi AI desktop robot with the ESP32 UNO D1 R32 development board.

## Hardware Requirements

- ESP32 UNO D1 R32 development board
- INMP441 MEMS microphone
- Max98357A I2S audio amplifier
- SSD1306 OLED display (128x64)
- 2 SG90 servos (horizontal and vertical)
- Speaker
- Breadboard and jumper wires

## Hardware Connections

### INMP441 Microphone Connections

The INMP441 is a high-quality I2S digital microphone. Connect it as follows:

| INMP441 Pin | ESP32 UNO D1 R32 Pin |
|------------|---------------------|
| VDD        | 3.3V                |
| GND        | GND                 |
| SD         | GPIO17              |
| L/R        | GND (left channel) or 3.3V (right channel) |
| WS         | GPIO16              |
| SCK        | GPIO14              |

### Max98357A Audio Amplifier Connections

Max98357A is an I2S audio amplifier. Connect it as follows:

| Max98357A Pin | ESP32 UNO D1 R32 Pin |
|-------------|---------------------|
| VIN         | 5V or 3.3V |
| GND         | GND                 |
| DIN         | GPIO25              |
| BCLK        | GPIO26              |
| LRC         | GPIO27              |
| GAIN        | Unconnected (default 0 dB gain) |
| SD          | Unconnected (do not connect to GND) |

**Note**: Testing showed that the MAX98357A produces sound only when its SD pin is left unconnected. Connecting it to GND results in no audio output.

### SSD1306 OLED Display Connections

| SSD1306 Pin | ESP32 UNO D1 R32 Pin |
|------------|---------------------|
| VCC        | 3.3V                |
| GND        | GND                 |
| SCL        | GPIO22              |
| SDA        | GPIO21              |

### Servo Connections

| Servo       | ESP32 UNO D1 R32 Pin |
|-----------|---------------------|
| Horizontal servo signal wire | GPIO19 |
| Vertical servo signal wire | GPIO18 |
| VCC        | 5V                  |
| GND        | GND                 |

### Touch Sensor Connections

#### Option 1: Use the ESP32’s Built-In Touch Sensor

| Connection | ESP32 UNO D1 R32 Pin |
|-------|---------------------|
| Metal plate/conductive material | GPIO4 (through a 10 kΩ resistor) |
| Pull-up resistor | 3.3V to GPIO4 (10 kΩ) |

Wiring diagram:
```
3.3V ---- 10K resistor ---- GPIO4 ---- metal plate/conductive material
```

#### Option 2: Use a Three-Wire Touch Switch Module (Default Configuration)

| Touch Module Pin | ESP32 UNO D1 R32 Pin |
|------------|---------------------|
| VCC (red wire)  | 5V or 3.3V |
| GND (black wire)  | GND |
| OUT (yellow wire)  | GPIO4 |

Wiring diagram:
```
ESP32 5V/3.3V ---- touch module VCC (red wire)
ESP32 GND    ---- touch module GND (black wire)
ESP32 GPIO4  ---- touch module OUT (yellow wire)
```

**Note**:
- The touch module’s VCC can be connected to 5V or 3.3V; either works.
- The code is currently configured to use the three-wire touch switch module by default.
- To use the ESP32’s built-in touch sensor, set TOUCH_SENSOR_TYPE to 0 in config.h.

## Features

1. **Voice interaction**: Voice input and output via the INMP441 microphone and Max98357A audio amplifier
2. **Expression display**: Shows different expressions and text on the SSD1306 OLED display
3. **Servo control**: A two-servo gimbal enables head movements such as nodding and shaking
4. **Xiaozhi AI integration**: Communicates with the Xiaozhi AI server for intelligent conversation and interaction

## Audio Configuration Optimizations

The following audio configuration optimizations address playback stuttering and crackling:

1. **Sample rate matching**: Set the output sample rate to 16000 Hz to match the input rate and avoid instability from resampling.

2. **DMA buffer optimization**:
   - Increase the number of DMA descriptors (AUDIO_DMA_DESC_NUM) to 12
   - Increase the number of DMA frames (AUDIO_DMA_FRAME_NUM) to 600
   - These changes provide larger buffers and reduce stuttering caused by insufficient buffering

3. **Batch processing optimization**:
   - Reduce the batch size (AUDIO_BATCH_SIZE) to 60
   - A smaller batch size can make audio processing smoother and reduce perceived stuttering

4. **Improved clock stability**:
   - Increase the master clock multiplier (AUDIO_MCLK_MULTIPLE) to 384
   - This improves clock stability and reduces crackling

5. **MAX98357A connection notes**:
   - Leave the SD pin unconnected; do not connect it to GND.
   - The GAIN pin can be left unconnected; the default gain is 0 dB.

These optimization parameters can be adjusted in `config.h` to suit different hardware configurations and audio requirements.

## Audio Troubleshooting Guide

### MAX98357A Amplifier Noise and Stuttering

If you experience noise or stuttering with the MAX98357A amplifier, try the following solutions:

1. **Check hardware connections**:
   - Make sure MAX98357A VDD is connected to a stable 3.3V supply.
   - Ensure a secure GND connection, preferably directly to ESP32 GND.
   - If there is an SD pin, ensure it is high (amplifier enabled).
   - Check that the I2S connections (BCLK, LRCLK, and DIN) are correct.

2. **Power supply noise**:
   - Try adding separate power filtering for MAX98357A: connect a 100 μF electrolytic capacitor and a 0.1 μF ceramic capacitor in parallel between VDD and GND.
   - Keep power wires short and thick to reduce impedance.
   - If using USB power, try an external power adapter.

3. **Grounding**:
   - Ensure all components share a common ground, but avoid ground loops.
   - Try a star-ground topology, with all ground connections joined at a single point.

4. **Software configuration**:
   - Current settings:
     - DMA descriptor count:4
     - DMA frame count:128
     - Batch size:32
   - If audio still stutters, try the following settings:
     - Increase the number of DMA descriptors to 8.
     - Increase the number of DMA frames to 256.
     - Set the batch size to 64.

5. **Audio wiring**:
   - Use shielded audio cable to connect the speaker.
   - Keep audio cables away from power and digital signal wires.
   - Try adding a ferrite bead to the audio cable to reduce interference.

### Suggested Audio Parameter Settings

Depending on the use case, try the following parameter combinations:

| Scenario | DMA descriptors | DMA frames | Batch size | Notes |
|------|----------|---------|------------|------|
| Low latency | 4 | 128 | 32 | For applications requiring a fast response |
| Balanced | 8 | 256 | 64 | Balances latency and stability |
| High stability | 16 | 512 | 128 | For long playback, with higher latency |

## Usage

1. Connect the components according to the hardware wiring instructions.
2. Build and flash the firmware to the ESP32 UNO D1 R32 development board.
3. Configure the device through the Xiaozhi AI backend.
4. Use the touch sensor to control the device:
   - **Briefly touch GPIO4**: Toggle chat mode on or off.
   - **Touch and hold GPIO4**: Start recording; release to stop recording and send the audio.
5. The BOOT button retains its original function (entering bootloader mode); it resets Wi-Fi settings only when the device has started and is not connected to Wi-Fi.

## Notes

1. Servos require a 5V supply; make sure the supply can provide sufficient current.
2. The OLED display and INMP441 microphone use 3.3V power.
3. Ensure I2C and I2S are connected correctly to avoid display or audio issues.
4. Configure the Wi-Fi connection and Xiaozhi AI account before first use.
