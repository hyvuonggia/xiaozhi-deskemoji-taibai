/**
 * @file config.h
 * @brief ESP32 UNO D1 R32 board configuration file
 * 
 * This file defines the pin configuration and other parameters for the ESP32 UNO D1 R32 board
 */

#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio sample rate configuration
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000  // Sample rate matched to the server to avoid resampling

// Audio buffer and DMA configuration
#define AUDIO_DMA_DESC_NUM       32     // Further increase the number of DMA descriptors to improve buffering
#define AUDIO_DMA_FRAME_NUM      1024   // Further increase the DMA frame count to reduce stutter caused by buffer underrun
#define AUDIO_BATCH_SIZE         128    // Tune the batch size to balance processing latency and smoothness

// I2S clock configuration
#define AUDIO_MCLK_MULTIPLE      256    // Use the standard 256 multiple for better compatibility

// Use Simplex I2S mode
#define AUDIO_I2S_METHOD_SIMPLEX

// INMP441 microphone I2S configuration
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_16  // WS (Word Select)
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_14  // SCK (Serial Clock)
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_17  // SD (Serial Data)

// MAX98357A audio amplifier I2S configuration
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_25  // DIN
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_26  // BCLK
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_27  // LRC
// Note: the MAX98357A SD pin should be left floating, not tied to GND, otherwise there is no audio output

// Audio volume setting (0-100)
#define AUDIO_VOLUME            90     // Raise the volume to a higher level to increase signal strength

// Touch sensor configuration
#define TOUCH_SENSOR_GPIO       GPIO_NUM_4   // Touch sensor pin
#define TOUCH_THRESHOLD         1000         // Touch threshold (capacitive touch sensor)
#define TOUCH_SENSOR_TYPE       1            // 0: ESP32 built-in capacitive touch sensor, 1: external TTP223 touch module

// Button configuration
#define BOOT_BUTTON_GPIO        GPIO_NUM_0   // Onboard BOOT button
#define ASR_BUTTON_GPIO         GPIO_NUM_19  // Speech recognition button (optional)
#define BUILTIN_LED_GPIO        GPIO_NUM_2   // Onboard LED

// Button long-press duration definition (milliseconds)
#define BUTTON_LONG_PRESS_TIME  1000         // 1 second

// Servo configuration
#define SERVO_HORIZONTAL_PIN    GPIO_NUM_19  // Horizontal servo control pin
#define SERVO_VERTICAL_PIN      GPIO_NUM_18  // Vertical servo control pin

// OLED display I2C configuration
#define DISPLAY_SDA_PIN GPIO_NUM_21
#define DISPLAY_SCL_PIN GPIO_NUM_22
#define DISPLAY_WIDTH   128

#if CONFIG_OLED_SSD1306_128X32
#define DISPLAY_HEIGHT  32
#elif CONFIG_OLED_SSD1306_128X64
#define DISPLAY_HEIGHT  64
#else
#define DISPLAY_HEIGHT  64  // Use 128x64 OLED by default
#endif

#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

#endif // _BOARD_CONFIG_H_
