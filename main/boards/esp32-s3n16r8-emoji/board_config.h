/**
 * @file board_config.h
 * @brief ESP32 S3 N16R8 Emoji development board configuration
 * 
 * This file defines the pin configuration and other hardware-related parameters for the ESP32 S3 N16R8 Emoji development board.
 */

#pragma once

#include "driver/gpio.h"  // Include the ESP-IDF GPIO header, used for the GPIO_NUM_xx macro definitions
#include "driver/ledc.h"  // Add the header that contains the ledc-related type definitions

// Development board name
#ifndef BOARD_NAME
#define BOARD_NAME "ESP32-S3N16R8-EMOJI"
#endif

// Display configuration
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_I2C_SDA_PIN GPIO_NUM_41
#define DISPLAY_I2C_SCL_PIN GPIO_NUM_42
#define DISPLAY_I2C_ADDR 0x3C

// // Gesture recognition module configuration
// #define GESTURE_I2C_SDA_PIN GPIO_NUM_41  // Gesture sensor SDA pin, shares the I2C bus with the display
// #define GESTURE_I2C_SCL_PIN GPIO_NUM_42  // Gesture sensor SCL pin, shares the I2C bus with the display
// #define GESTURE_I2C_ADDR 0x73           // PAJ7620U2 sensor I2C address
// #define GESTURE_I2C_FREQ_HZ 100000      // I2C communication frequency, set to 100kHz to ensure stability

// Servo pin configuration
#define SERVO_HORIZONTAL_PIN GPIO_NUM_11  // Horizontal servo connected to GPIO11
#define SERVO_VERTICAL_PIN GPIO_NUM_12    // Vertical servo connected to GPIO12

// Servo angle range configuration - based on the boardemoji.ino settings, but the range is limited to protect the servos
#define SERVO_CENTER_X 90    // Horizontal servo center position (90 degrees)
#define SERVO_CENTER_Y 90     // Vertical servo center position (90 degrees)
#define SERVO_MIN_X (SERVO_CENTER_X - 40)  // Horizontal servo minimum angle (left boundary) - 40 degrees left of center
#define SERVO_MAX_X (SERVO_CENTER_X + 40)  // Horizontal servo maximum angle (right boundary) - 40 degrees right of center
#define SERVO_MIN_Y (SERVO_CENTER_Y - 20)  // Vertical servo minimum angle (top boundary) - 20 degrees above center
#define SERVO_MAX_Y (SERVO_CENTER_Y + 20)  // Vertical servo maximum angle (bottom boundary) - 20 degrees below center

// Step and delay values from the original boardemoji.ino
#define SERVO_STEP 1           // STEP value from Arduino
#define SERVO_DELAY 10         // SERVO_DELAY value from Arduino

// Servo PWM configuration, based on the original Arduino code settings
#define SERVO_MIN_PULSEWIDTH 500        // Minimum pulse width (microseconds)
#define SERVO_MAX_PULSEWIDTH 2500       // Maximum pulse width (microseconds)
#define LEDC_TIMER_BIT_WIDTH LEDC_TIMER_14_BIT  // 14-bit resolution
#define LEDC_TIMER LEDC_TIMER_0         // Use timer 0
#define LEDC_MODE LEDC_LOW_SPEED_MODE   // Use low-speed mode
#define LEDC_FREQUENCY 50               // PWM frequency 50Hz

// Servo channel configuration
#define SERVO_CHANNEL_COUNT 2                      // Number of servo channels
const ledc_channel_t servo_channels[SERVO_CHANNEL_COUNT] = {
    LEDC_CHANNEL_0,  // Horizontal servo uses channel 0
    LEDC_CHANNEL_1   // Vertical servo uses channel 1
};

// Servo pin array configuration
const uint8_t servo_pins[SERVO_CHANNEL_COUNT] = {
    SERVO_HORIZONTAL_PIN,  // Horizontal servo
    SERVO_VERTICAL_PIN,    // Vertical servo
};

// Button configuration
#define BOOT_BUTTON_PIN GPIO_NUM_0        // BOOT button connected to GPIO0
#define VOLUME_UP_BUTTON_PIN GPIO_NUM_40   // Volume up button connected to GPIO40
#define VOLUME_DOWN_BUTTON_PIN GPIO_NUM_39 // Volume down button connected to GPIO39

// Audio configuration - INMP441 microphone + MAX98357A audio amplifier
// INMP441 microphone pin connections:
// - VDD: 3.3V
// - GND: GND
// - SD: GPIO6 (I2S_DATA_IN_PIN)
// - L/R: GND (grounded, operates in left channel mode)
// - WS: GPIO4 (I2S_MIC_WS_PIN)
// - SCK: GPIO5 (I2S_MIC_SCK_PIN)
//
// MAX98357A audio amplifier pin connections:
// - VIN: 3.3V
// - GND: GND
// - DIN: GPIO7 (I2S_DATA_OUT_PIN)
// - BCLK: GPIO15 (I2S_SPEAKER_BCLK_PIN)
// - LRC: GPIO16 (I2S_SPEAKER_WS_PIN)
// - GAIN: GND
// - SD: 3.3V (enable pin, enabled by a high level)

// Audio clock configuration
#define I2S_SPEAKER_BCLK_PIN GPIO_NUM_15  // Audio amplifier clock (MAX98357A BCLK)
#define I2S_MIC_SCK_PIN GPIO_NUM_5       // Microphone clock (INMP441 SCK)

// Define the WS pins for the microphone and speaker separately
#define I2S_MIC_WS_PIN GPIO_NUM_4        // Microphone WS (INMP441 WS)
#define I2S_SPEAKER_WS_PIN GPIO_NUM_16   // Audio amplifier WS (MAX98357A LRC)

// Data pins
#define I2S_DATA_OUT_PIN GPIO_NUM_7       // I2S data output line, connected to GPIO7 (speaker DIN)
#define I2S_DATA_IN_PIN GPIO_NUM_6        // I2S data input line, connected to GPIO6 (microphone SD)

// Audio sample rate configuration
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// LED configuration
#define LED_PIN GPIO_NUM_48                // Onboard LED connected to GPIO48
