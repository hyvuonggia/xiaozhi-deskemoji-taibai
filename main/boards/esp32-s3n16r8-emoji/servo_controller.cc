/**
 * @file servo_controller.cc
 * @brief Servo control module implementation
 */

#include "servo_controller.h"
#include <esp_log.h>

#define TAG "ServoController"

ServoController::ServoController() {
}

ServoController::~ServoController() {
}

void ServoController::Initialize() {
    // Configure the LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_TIMER_BIT_WIDTH,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure the LEDC channels
    for (int i = 0; i < SERVO_CHANNEL_COUNT; i++) {
        ledc_channel_config_t ledc_channel = {
            .gpio_num = servo_pins[i],
            .speed_mode = LEDC_MODE,
            .channel = servo_channels[i],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER,
            .duty = 0,
            .hpoint = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    // Set the initial servo positions
    SetServoAngle(0, SERVO_CENTER_X);
    SetServoAngle(1, SERVO_CENTER_Y);
}

void ServoController::SetServoAngle(int channel, int angle) {
    // Clamp the angle to the valid range
    if (channel == 0) { // Horizontal servo
        angle = std::max(SERVO_MIN_X, std::min(SERVO_MAX_X, angle));
        current_x_angle_ = angle;
    } else { // Vertical servo
        angle = std::max(SERVO_MIN_Y, std::min(SERVO_MAX_Y, angle));
        current_y_angle_ = angle;
    }

    // Compute the PWM duty cycle
    uint32_t pulse_width = SERVO_MIN_PULSEWIDTH + (angle * (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH)) / 180;
    uint32_t duty = (pulse_width * ((1 << LEDC_TIMER_BIT_WIDTH) - 1)) / (1000000 / LEDC_FREQUENCY);

    // Set the PWM duty cycle
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, servo_channels[channel], duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, servo_channels[channel]));
}

void ServoController::HeadMove(int x_offset, int y_offset, int servo_delay) {
    // Get the current angles
    int x_angle = current_x_angle_;
    int y_angle = current_y_angle_;
    
    // Compute the target angles
    int to_x_angle = std::max(SERVO_MIN_X, std::min(SERVO_MAX_X, x_angle + x_offset));
    int to_y_angle = std::max(SERVO_MIN_Y, std::min(SERVO_MAX_Y, y_angle + y_offset));
    
    // Move step by step to the target position, closely following the implementation in boardemoji.ino
    while (x_angle != to_x_angle || y_angle != to_y_angle) {
        if (x_angle != to_x_angle) {
            x_angle += (to_x_angle > x_angle ? SERVO_STEP : -SERVO_STEP);
            SetServoAngle(0, x_angle);
        }
        if (y_angle != to_y_angle) {
            y_angle += (to_y_angle > y_angle ? SERVO_STEP : -SERVO_STEP);
            SetServoAngle(1, y_angle);
        }
        vTaskDelay(pdMS_TO_TICKS(servo_delay));
    }
}

void ServoController::HeadNod(int servo_delay) {
    // Ensure a minimum delay value
    int actual_delay = std::max(10, servo_delay);
    
    // Adjust the nod frequency according to the delay value
    int delay_ms = actual_delay * 15; // Convert the delay value to milliseconds
    
    // Record the initial position
    int initial_y = current_y_angle_;
    
    // Set the servo angle directly instead of using HeadMove's step-by-step motion
    for (int i = 0; i < 3; i++) {
        // Nod down
        SetServoAngle(1, SERVO_CENTER_Y + 20);
        current_y_angle_ = SERVO_CENTER_Y + 20;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));  // Delay controlled by the parameter
        
        // Nod up
        SetServoAngle(1, SERVO_CENTER_Y - 20);
        current_y_angle_ = SERVO_CENTER_Y - 20;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));  // Delay controlled by the parameter
    }
    
    // Return to the center position
    SetServoAngle(1, SERVO_CENTER_Y);
    current_y_angle_ = SERVO_CENTER_Y;
}

void ServoController::HeadShake(int servo_delay) {
    // Ensure a minimum delay value
    int actual_delay = std::max(10, servo_delay);
    
    // Adjust the shake frequency according to the delay value
    int delay_ms = actual_delay * 15; // Convert the delay value to milliseconds
    
    // Record the initial position
    int initial_x = current_x_angle_;
    
    // Set the servo angle directly instead of using HeadMove's step-by-step motion
    for (int i = 0; i < 2; i++) {
        // Shake left
        SetServoAngle(0, SERVO_CENTER_X - 20);
        current_x_angle_ = SERVO_CENTER_X - 20;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));  // Delay controlled by the parameter
        
        // Shake right
        SetServoAngle(0, SERVO_CENTER_X + 20);
        current_x_angle_ = SERVO_CENTER_X + 20;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));  // Delay controlled by the parameter
        
        // Shake left again
        SetServoAngle(0, SERVO_CENTER_X - 20);
        current_x_angle_ = SERVO_CENTER_X - 20;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));  // Delay controlled by the parameter
    }
    
    // Return to the center position
    SetServoAngle(0, SERVO_CENTER_X);
    current_x_angle_ = SERVO_CENTER_X;
}

void ServoController::HeadRoll(int servo_delay) {
    // Closely follows the implementation in boardemoji.ino
    HeadCenter();
    HeadDown(SERVO_OFFSET_Y/2+5);
    HeadMove(SERVO_OFFSET_X, -SERVO_OFFSET_Y/2, servo_delay);
    HeadMove(-SERVO_OFFSET_X, -SERVO_OFFSET_Y/2, servo_delay);
    HeadMove(-SERVO_OFFSET_X, SERVO_OFFSET_Y/2, servo_delay);
    HeadMove(SERVO_OFFSET_X, SERVO_OFFSET_Y/2, servo_delay);
    HeadMove(-SERVO_OFFSET_X, -SERVO_OFFSET_Y/2, servo_delay);
    HeadMove(SERVO_OFFSET_X, -SERVO_OFFSET_Y/2, servo_delay);
    HeadMove(SERVO_OFFSET_X, SERVO_OFFSET_Y/2, servo_delay);
    HeadMove(-SERVO_OFFSET_X, SERVO_OFFSET_Y/2, servo_delay);
    HeadCenter();
}

void ServoController::HeadUp(int offset) {
    HeadMove(0, -offset);
}

void ServoController::HeadDown(int offset) {
    HeadMove(0, offset);
}

void ServoController::HeadLeft(int offset) {
    HeadMove(-offset, 0);
}

void ServoController::HeadRight(int offset) {
    HeadMove(offset, 0);
}

void ServoController::HeadCenter(int servo_delay) {
    // Based on the implementation in the original boardemoji.ino, moves step by step to the center using HeadMove
    // Compute the offset from the current position to the center
    int x_offset = SERVO_CENTER_X - current_x_angle_;
    int y_offset = SERVO_CENTER_Y - current_y_angle_;
    
    // Move step by step to the center using HeadMove
    HeadMove(x_offset, y_offset, servo_delay);
}
