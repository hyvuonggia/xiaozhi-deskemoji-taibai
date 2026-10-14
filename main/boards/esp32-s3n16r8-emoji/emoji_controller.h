/**
 * @file emoji_controller.h
 * @brief Emoji control module header
 * 
 * This file defines the interface of the emoji control module, which drives the expression display on the OLED screen
 */

#pragma once

#include <lvgl.h>
#include "display/display.h"
#include "board_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <functional>  // for std::function
#include <cstring>     // for string operations
#include "servo_controller.h"  // include the servo controller header

// Animation type enum
enum class AnimationType {
    BLINK,      // blink
    HAPPY,      // happy
    SAD,        // sad
    ANGER,      // angry
    SURPRISE,   // surprised
    WAKEUP,     // wake up
    SLEEP,      // sleep
    LOOK_LEFT,  // look left
    LOOK_RIGHT, // look right
    HEAD_NOD,   // nod the head
    HEAD_SHAKE, // shake the head
    HEAD_ROLL,  // roll the head
    CONFUSED,   // confused
    AWKWARD,    // awkward
    CRY,        // crying
    LAUGHING,   // laughing
    FUNNY,      // funny
    LOVING,     // loving
    EMBARRASSED, // awkward
    SHOCKED,    // shocked
    THINKING,   // thinking
    COOL,       // cool
    RELAXED,    // relaxed
    DELICIOUS,  // delicious
    KISSY,      // kissing
    CONFIDENT,  // confident
    SILLY,      // silly
    RANDOM      // random animation
};

// Animation message struct
struct AnimationMessage {
    AnimationType type;  // animation type
    int param;          // animation parameter (e.g. speed)
};

/**
 * @class EmojiController
 * @brief Emoji control class, responsible for managing and displaying the various expressions
 */
class EmojiController {
public:
    /**
     * @brief Constructor
     * @param display Pointer to the display object
     */
    EmojiController(Display* display);
    
    /**
     * @brief Destructor
     */
    ~EmojiController();
    
    /**
     * @brief Initialise the emoji controller
     */
    void Initialize();
    
    /**
     * @brief Create the emoji-mode screen
     * @return LVGL screen object
     */
    lv_obj_t* CreateEmojiScreen();
    
    /**
     * @brief Clean up the emoji-mode screen
     */
    void CleanupEmojiScreen();
    
    /**
     * @brief Draw the emoji
     * @param is_blinking Whether the eyes are currently blinking
     */
    void DrawEmoji(bool is_blinking);
    
    /**
     * @brief Centre the eyes
     * @param update_display Whether to refresh the display
     */
    void EyeCenter(bool update_display = true);
    
    /**
     * @brief Blink animation
     * @param speed Blink speed
     */
    void EyeBlink(int speed = 12);
    
    /**
     * @brief Sleeping expression
     */
    void EyeSleep();
    
    /**
     * @brief Wake-up expression
     */
    void EyeWakeup();
    
    /**
     * @brief Happy expression
     */
    void EyeHappy();
    
    /**
     * @brief Sad expression
     */
    void EyeSad();
    
    /**
     * @brief Angry expression
     */
    void EyeAnger();
    
    /**
     * @brief Surprised expression
     */
    void EyeSurprise();
    
    /**
     * @brief Eyes up
     */
    void EyeUp();
    
    /**
     * @brief Eyes down
     */
    void EyeDown();
    
    /**
     * @brief Eyes right
     */
    void EyeRight();
    
    /**
     * @brief Eyes left
     */
    void EyeLeft();
    
    /**
     * @brief Eye saccade
     * @param direction_x X direction
     * @param direction_y Y direction
     */
    void Saccade(int direction_x, int direction_y);
    
    /**
     * @brief Move the eyes
     * @param direction Direction
     */
    void MoveEye(int direction);
    
    /**
     * @brief Stop all animations
     */
    void StopAnimation();
    
    /**
     * @brief Suspend the LVGL task, lowering the refresh rate to avoid triggering the watchdog
     */
    void SuspendLVGLTask();
    
    /**
     * @brief Resume the LVGL task
     */
    void ResumeLVGLTask();
    
    /**
     * @brief Helper that runs an animation safely
     * @param animation_func The animation function to run
     * @return Whether the animation ran successfully
     */
    bool SafeExecuteAnimation(std::function<void()> animation_func);
    
    /**
     * @brief Play an animation
     * @param type Animation type
     * @param param Animation parameter
     */
    void PlayAnimation(AnimationType type, int param = 0);
    
    /**
     * @brief Start the blink timer (legacy-compatible interface)
     */
    void StartBlinkTimer() {
        if (blink_timer_) {
            xTimerStart(blink_timer_, 0);
        }
    }
    
    /**
     * @brief Stop the blink timer (legacy-compatible interface)
     */
    void StopBlinkTimer() {
        if (blink_timer_) {
            xTimerStop(blink_timer_, 0);
        }
        if (emoji_timer_) {
            xTimerStop(emoji_timer_, 0);
        }
        StopAnimation();
    }
    
    /**
     * @brief Set the servo controller
     * @param servo_controller Pointer to the servo controller
     */
    void SetServoController(class ServoController* servo_controller) {
        servo_controller_ = servo_controller;
    }
    
    /**
     * @brief Get the emoji screen
     * @return LVGL screen object
     */
    lv_obj_t* GetEmojiScreen() const { return emoji_screen_; }
    
    /**
     * @brief Set the blinking state
     * @param blinking Whether the eyes are blinking
     */
    void SetBlinking(bool blinking) { is_blinking_ = blinking; }
    
    /**
     * @brief Get the blinking state
     * @return Whether the eyes are blinking
     */
    bool IsBlinking() const { return is_blinking_; }
    
    /**
     * @brief Enable or disable random animation
     * @param enabled Whether random animation is enabled
     */
    void SetRandomAnimationEnabled(bool enabled);
    
    /**
     * @brief Clear the animation queue
     */
    void ClearAnimationQueue();
    
    /**
     * @brief Test the facial expressions
     */
    void TestFacialExpressions();

private:
    // Reference values
    int ref_eye_width_ = 40;
    int ref_eye_height_ = 40;
    int ref_space_between_eye_ = 10;
    int ref_corner_radius_ = 10;
    
    // Current eye state
    int left_eye_height_ = ref_eye_height_;
    int left_eye_width_ = ref_eye_width_;
    int right_eye_height_ = ref_eye_height_;
    int right_eye_width_ = ref_eye_width_;
    int left_eye_x_ = 32;  // matches boardemoji.ino
    int left_eye_y_ = 32;  // matches boardemoji.ino
    int right_eye_x_ = 32 + ref_eye_width_ + ref_space_between_eye_;  // matches boardemoji.ino
    int right_eye_y_ = 32;  // matches boardemoji.ino
    
    // Original eye dimensions, restored once an animation finishes
    int original_left_width_ = ref_eye_width_;
    int original_left_height_ = ref_eye_height_;
    int original_right_width_ = ref_eye_width_;
    int original_right_height_ = ref_eye_height_;
    
    // Emoji-mode related variables
    bool is_blinking_ = false;
    TimerHandle_t blink_timer_ = nullptr;
    TimerHandle_t emoji_timer_ = nullptr;
    
    // Animation queue and tasks
    QueueHandle_t animation_queue_ = nullptr;
    TaskHandle_t animation_task_handle_ = nullptr;
    TaskHandle_t animation_timer_task_handle_ = nullptr;
    
    // Random animation control
    bool random_animation_enabled_ = true;
    TickType_t last_random_animation_time_ = 0;
    
    // Animation-running flag, used to keep several animations from running at once
    bool is_animating_ = false;
    
    // LVGL task control
    bool lvgl_task_suspended_ = false;
    UBaseType_t saved_lvgl_task_priority_ = 0;
    
    // Constants
    static constexpr int BLINK_INTERVAL_MS = 5000;  // blink interval: once every 5 seconds
    static constexpr int BLINK_DURATION_MS = 500;   // blink duration
    static constexpr int ANIMATION_QUEUE_SIZE = 10; // animation queue size
    static constexpr int RANDOM_ANIMATION_INTERVAL_MS = 10000; // interval between random animations
    
    // Servo offsets use the macros defined in servo_controller.h
    // SERVO_OFFSET_X and SERVO_OFFSET_Y are already defined in servo_controller.h
    
    // LVGL objects
    lv_obj_t* emoji_screen_ = nullptr;  // dedicated screen for emoji mode
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    
    // Display object
    Display* display_ = nullptr;
    
    // Servo controller
    class ServoController* servo_controller_ = nullptr;
    
    // Timer callbacks
    static void BlinkTimerCallback(TimerHandle_t timer);
    static void EmojiTimerCallback(TimerHandle_t timer);
    
    // Animation task functions
    static void AnimationTaskFunction(void* pvParameters);
    static void AnimationTimerTask(void* pvParameters);
    
    // Internal methods that run each concrete animation
    void ExecuteBlinkAnimation(int speed);
    void ExecuteHappyAnimation();
    void ExecuteSadAnimation();
    void ExecuteAngerAnimation();
    void ExecuteSurpriseAnimation();
    void ExecuteWakeupAnimation();
    void ExecuteSleepAnimation();
    void ExecuteAwkwardAnimation();
    void ExecuteCryAnimation();  // crying expression
    
    // Additional animation methods
    void ExecuteLaughingAnimation();  // laughing expression
    void ExecuteFunnyAnimation();     // funny expression
    void ExecuteLovingAnimation();    // loving expression
    void ExecuteEmbarrassedAnimation(); // embarrassed expression
    void ExecuteShockedAnimation();   // shocked expression
    void ExecuteThinkingAnimation();  // thinking expression
    void ExecuteCoolAnimation();      // cool expression
    void ExecuteRelaxedAnimation();   // relaxed expression
    void ExecuteDeliciousAnimation(); // delicious expression
    void ExecuteKissyAnimation();     // kissing expression
    void ExecuteConfidentAnimation(); // confident expression
    void ExecuteSillyAnimation();     // silly expression
    
    // Declarations added for previously missing functions
    void EyeConfused();
    void ExecuteConfusedAnimation();
    void ExecuteLookLeftAnimation();
    void ExecuteLookRightAnimation();
    void ExecuteHeadNodAnimation();
    void ExecuteHeadShakeAnimation();
    void ExecuteHeadRollAnimation();
    void InitEmoji();
    static void AnimationTask(void* pvParameters);
    AnimationType SelectRandomAnimation();
    void PlayRandomAnimation();
    void ExecuteRandomAnimation();
};
