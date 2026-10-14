/**
 * @file servo_controller.h
 * @brief Servo control module header file
 * 
 * This file defines the servo control module interface, used to control horizontal and vertical servo motion.
 */

#pragma once

#include "board_config.h"
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>

// Servo-related constants, using the constants already defined in board_config.h
// Note: SERVO_CENTER_X, SERVO_CENTER_Y, SERVO_MIN_X, SERVO_MAX_X, SERVO_MIN_Y, SERVO_MAX_Y, SERVO_STEP, SERVO_DELAY
// are already defined in board_config.h

// Servo offsets, used by functions such as HeadRoll
#define SERVO_OFFSET_X 40
#define SERVO_OFFSET_Y 25

/**
 * @class ServoController
 * @brief Servo control class, responsible for managing and controlling servo motion
 */
class ServoController {
public:
    /**
     * @brief Constructor
     */
    ServoController();
    
    /**
     * @brief Destructor
     */
    ~ServoController();
    
    /**
     * @brief Initialize the servo controller
     */
    void Initialize();
    
    /**
     * @brief Set the servo angle
     * @param channel Servo channel (0: horizontal, 1: vertical)
     * @param angle Angle (0-180)
     */
    void SetServoAngle(int channel, int angle);
    
    /**
     * @brief Move the head
     * @param x_offset X-axis offset
     * @param y_offset Y-axis offset
     * @param servo_delay Servo movement delay
     */
    void HeadMove(int x_offset, int y_offset, int servo_delay = SERVO_DELAY);
    
    /**
     * @brief Move the head up
     * @param offset Offset
     */
    void HeadUp(int offset = 20);
    
    /**
     * @brief Move the head down
     * @param offset Offset
     */
    void HeadDown(int offset = 20);
    
    /**
     * @brief Move the head left
     * @param offset Offset
     */
    void HeadLeft(int offset = 20);
    
    /**
     * @brief Move the head right
     * @param offset Offset
     */
    void HeadRight(int offset = 20);
    
    /**
     * @brief Center the head
     * @param servo_delay Servo movement delay
     */
    void HeadCenter(int servo_delay = SERVO_DELAY);
    
    /**
     * @brief Head nod
     * @param servo_delay Servo movement delay
     */
    void HeadNod(int servo_delay = 1);
    
    /**
     * @brief Head shake
     * @param servo_delay Servo movement delay
     */
    void HeadShake(int servo_delay = 1);
    
    /**
     * @brief Head roll
     * @param servo_delay Servo movement delay
     */
    void HeadRoll(int servo_delay = SERVO_DELAY);
    
    /**
     * @brief Get the current horizontal angle
     * @return Horizontal angle
     */
    int GetCurrentXAngle() const { return current_x_angle_; }
    
    /**
     * @brief Get the current vertical angle
     * @return Vertical angle
     */
    int GetCurrentYAngle() const { return current_y_angle_; }

private:
    // Servo angles
    int current_x_angle_ = SERVO_CENTER_X;
    int current_y_angle_ = SERVO_CENTER_Y;
};
