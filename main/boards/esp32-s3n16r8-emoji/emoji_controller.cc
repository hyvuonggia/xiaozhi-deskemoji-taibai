/**
 * @file emoji_controller.cc
 * @brief Emoji control module implementation
 */

#include "emoji_controller.h"
#include <esp_log.h>
#include <cmath>       // add math function support such as cos and sin
#include <esp_random.h> // add random number generator support
#include <cstring>     // add strcmp function support
#include <functional>  // add std::function support

#define TAG "EmojiController"
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

EmojiController::EmojiController(Display* display) : display_(display) {
}

EmojiController::~EmojiController() {
    if (animation_task_handle_) {
        vTaskDelete(animation_task_handle_);
    }
    if (animation_timer_task_handle_) {
        vTaskDelete(animation_timer_task_handle_);
    }
    
    CleanupEmojiScreen();
}

void EmojiController::Initialize() {
    ESP_LOGI(TAG, "Initializing emoji controller");
    
    // create the animation queue
    animation_queue_ = xQueueCreate(ANIMATION_QUEUE_SIZE, sizeof(AnimationMessage));
    
    // create the animation task
    BaseType_t task_created = xTaskCreate(
        AnimationTask,    // task function
        "AnimationTask",  // task name
        4096,            // stack size
        this,            // task parameter
        5,               // task priority
        &animation_task_handle_  // task handle
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create animation task");
    }
    
    // create the animation timer task
    task_created = xTaskCreate(
        AnimationTimerTask,        // task function
        "AnimationTimerTask",      // task name
        4096,                      // stack size
        this,                      // task parameter
        5,                         // task priority
        &animation_timer_task_handle_       // task handle
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create animation timer task");
    }
}

// timer task function, replaces the FreeRTOS timer
void EmojiController::AnimationTimerTask(void* pvParameters) {
    EmojiController* controller = static_cast<EmojiController*>(pvParameters);
    if (controller == nullptr) {
        ESP_LOGE(TAG, "AnimationTimerTask: invalid controller pointer");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "AnimationTimerTask: started");
    
    // record the time of the last random animation
    TickType_t last_random_time = xTaskGetTickCount();
    
    while (true) {
        // current time
        TickType_t current_time = xTaskGetTickCount();
        
        // check whether a random animation should run
        if (controller->random_animation_enabled_ && 
            (current_time - last_random_time) >= pdMS_TO_TICKS(RANDOM_ANIMATION_INTERVAL_MS)) {
            ESP_LOGI(TAG, "AnimationTimerTask: Preparing to execute random animation");
            
            // check again whether random animation is enabled (to prevent the state changing before the message is sent)
            if (controller->random_animation_enabled_) {
                // update the time of the last random animation
                last_random_time = current_time;
                
                // send the random animation message
                if (controller->animation_queue_ != nullptr) {
                    AnimationMessage msg;
                    msg.type = AnimationType::RANDOM;
                    msg.param = 0;
                    
                    if (xQueueSend(controller->animation_queue_, &msg, 0) != pdPASS) {
                        ESP_LOGW(TAG, "AnimationTimerTask: Failed to send random animation message");
                    } else {
                        ESP_LOGI(TAG, "AnimationTimerTask: Random animation message sent");
                    }
                }
            } else {
                ESP_LOGI(TAG, "AnimationTimerTask: Random animation is disabled; not sending a message");
                // still update the time to avoid checking continuously
                last_random_time = current_time;
            }
        }
        
        // delay for a while
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void EmojiController::AnimationTask(void* pvParameters) {
    EmojiController* controller = static_cast<EmojiController*>(pvParameters);
    if (controller == nullptr) {
        ESP_LOGE(TAG, "AnimationTask: invalid controller pointer");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "AnimationTask: started");
    
    AnimationMessage msg;
    
    while (true) {
        // wait for an animation message
        if (xQueueReceive(controller->animation_queue_, &msg, portMAX_DELAY) == pdPASS) {
            // if it is a random animation but random animation is disabled, skip it
            if (msg.type == AnimationType::RANDOM && !controller->random_animation_enabled_) {
                ESP_LOGI(TAG, "AnimationTask: Random animation is disabled; skipping execution");
                continue;
            }
            
            ESP_LOGI(TAG, "AnimationTask: Received animation message, type: %d, parameter: %d", (int)msg.type, msg.param);
            
            // execute the corresponding animation based on the animation type
            // before executing the animation, check whether another animation is running
            if (controller->is_animating_) {
                ESP_LOGW(TAG, "AnimationTask: An animation is already running; skipping this animation");
                continue;
            }
            
            // set the animation executing flag
            controller->is_animating_ = true;
            
            // suspend the LVGL task to reduce the refresh rate
            controller->SuspendLVGLTask();
            
            try {
                // execute the corresponding animation
                switch (msg.type) {
                    case AnimationType::BLINK:
                        controller->ExecuteBlinkAnimation(msg.param);
                        break;
                    case AnimationType::HAPPY:
                        controller->ExecuteHappyAnimation();
                        break;
                    case AnimationType::SAD:
                        controller->ExecuteSadAnimation();
                        break;
                    case AnimationType::ANGER:
                        controller->ExecuteAngerAnimation();
                        break;
                    case AnimationType::SURPRISE:
                        controller->ExecuteSurpriseAnimation();
                        break;
                    case AnimationType::WAKEUP:
                        controller->ExecuteWakeupAnimation();
                        break;
                    case AnimationType::SLEEP:
                        controller->ExecuteSleepAnimation();
                        break;
                    case AnimationType::LOOK_LEFT:
                        controller->ExecuteLookLeftAnimation();
                        break;
                    case AnimationType::LOOK_RIGHT:
                        controller->ExecuteLookRightAnimation();
                        break;
                    case AnimationType::HEAD_NOD:
                        controller->ExecuteHeadNodAnimation();
                        break;
                    case AnimationType::HEAD_SHAKE:
                        controller->ExecuteHeadShakeAnimation();
                        break;
                    case AnimationType::HEAD_ROLL:
                        controller->ExecuteHeadRollAnimation();
                        break;
                    case AnimationType::CONFUSED:
                        controller->ExecuteConfusedAnimation();
                        break;
                    case AnimationType::AWKWARD:
                        controller->ExecuteAwkwardAnimation();
                        break;
                    case AnimationType::CRY:
                        controller->ExecuteCryAnimation();
                        break;
                    case AnimationType::LAUGHING:
                        controller->ExecuteLaughingAnimation();
                        break;
                    case AnimationType::FUNNY:
                        controller->ExecuteFunnyAnimation();
                        break;
                    case AnimationType::LOVING:
                        controller->ExecuteLovingAnimation();
                        break;
                    case AnimationType::EMBARRASSED:
                        controller->ExecuteEmbarrassedAnimation();
                        break;
                    case AnimationType::SHOCKED:
                        controller->ExecuteShockedAnimation();
                        break;
                    case AnimationType::THINKING:
                        controller->ExecuteThinkingAnimation();
                        break;
                    case AnimationType::COOL:
                        controller->ExecuteCoolAnimation();
                        break;
                    case AnimationType::RELAXED:
                        controller->ExecuteRelaxedAnimation();
                        break;
                    case AnimationType::DELICIOUS:
                        controller->ExecuteDeliciousAnimation();
                        break;
                    case AnimationType::KISSY:
                        controller->ExecuteKissyAnimation();
                        break;
                    case AnimationType::CONFIDENT:
                        controller->ExecuteConfidentAnimation();
                        break;
                    case AnimationType::SILLY:
                        controller->ExecuteSillyAnimation();
                        break;
                    case AnimationType::RANDOM:
                        controller->ExecuteRandomAnimation();
                        break;
                    default:
                        ESP_LOGW(TAG, "AnimationTask: Unknown animation type: %d", (int)msg.type);
                        break;
                }
            } catch (const std::exception& e) {
                ESP_LOGE(TAG, "AnimationTask exception: %s", e.what());
            } catch (...) {
                ESP_LOGE(TAG, "Unknown AnimationTask exception");
            }
            
            // resume the LVGL task
            controller->ResumeLVGLTask();
            
            // clear the animation executing flag
            controller->is_animating_ = false;
            
            // add a short pause after each animation completes to give the system some time to recover
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    
    // note: this is never reached because the loop above is infinite
    vTaskDelete(NULL);
}

lv_obj_t* EmojiController::CreateEmojiScreen() {
    if (emoji_screen_ != nullptr) return emoji_screen_;

    emoji_screen_ = lv_obj_create(nullptr);  // create a new screen
    lv_obj_set_size(emoji_screen_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(emoji_screen_, lv_color_white(), 0);  // set a white background, which actually renders as black
    lv_obj_set_style_border_width(emoji_screen_, 0, 0);  // no border
    
    // create the left eye
    left_eye_ = lv_obj_create(emoji_screen_);
    lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
    lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
    lv_obj_set_style_radius(left_eye_, ref_corner_radius_, 0);
    lv_obj_set_style_bg_color(left_eye_, lv_color_black(), 0);  // set black eyes, which actually render as white
    lv_obj_set_style_border_width(left_eye_, 0, 0);
    
    // create the right eye
    right_eye_ = lv_obj_create(emoji_screen_);
    lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
    lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
    lv_obj_set_style_radius(right_eye_, ref_corner_radius_, 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_black(), 0);  // set black eyes, which actually render as white
    lv_obj_set_style_border_width(right_eye_, 0, 0);
    
    return emoji_screen_;
}

void EmojiController::CleanupEmojiScreen() {
    if (emoji_screen_ != nullptr) {
        lv_obj_del(emoji_screen_);
        emoji_screen_ = nullptr;
        left_eye_ = nullptr;
        right_eye_ = nullptr;
    }
}

void EmojiController::DrawEmoji(bool is_blinking) {
    is_blinking_ = is_blinking;
    
    // make sure the screen and eye objects exist
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "DrawEmoji: screen or eye object does not exist");
        return;
    }
    
    // try to acquire the lock
    bool lock_success = false;
    int retry_count = 0;
    const int max_retries = 3;

    while (!lock_success && retry_count < max_retries) {
        if (display_) {
            try {
                DisplayLockGuard lock(display_);
                
                // update the eye position and size
                lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
                lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
                lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
                lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
                
                lock_success = true;
            } catch (...) {
                ESP_LOGW(TAG, "DrawEmoji: Failed to acquire display lock, retry %d/%d", retry_count + 1, max_retries);
                retry_count++;
                vTaskDelay(pdMS_TO_TICKS(10)); // retry after a short delay
            }
        } else {
            ESP_LOGW(TAG, "DrawEmoji: display object does not exist");
            return;
        }
    }

    if (!lock_success) {
        ESP_LOGE(TAG, "DrawEmoji: Failed to acquire display lock after multiple attempts");
    }
}

void EmojiController::EyeCenter(bool update_display) {
    // restore the eyes to the center position and default size
    left_eye_x_ = DISPLAY_WIDTH / 2 - ref_eye_width_ / 2 - ref_space_between_eye_ / 2;
    left_eye_y_ = DISPLAY_HEIGHT / 2;
    left_eye_width_ = ref_eye_width_;
    left_eye_height_ = ref_eye_height_;
    
    right_eye_x_ = DISPLAY_WIDTH / 2 + ref_eye_width_ / 2 + ref_space_between_eye_ / 2;
    right_eye_y_ = DISPLAY_HEIGHT / 2;
    right_eye_width_ = ref_eye_width_;
    right_eye_height_ = ref_eye_height_;
    
    if (update_display) {
        if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
            ESP_LOGW(TAG, "EyeCenter: screen or eye object does not exist");
            return;
        }
        
        if (display_) {
            try {
                DisplayLockGuard lock(display_);
                
                // update the eye objects directly
                lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
                lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
                lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
                lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
                
                // restore the corner radius
                lv_obj_set_style_radius(left_eye_, ref_corner_radius_, 0);
                lv_obj_set_style_radius(right_eye_, ref_corner_radius_, 0);
            } catch (...) {
                ESP_LOGW(TAG, "EyeCenter: Failed to acquire display lock");
            }
        }
    }
}

void EmojiController::EyeBlink(int speed) {
    PlayAnimation(AnimationType::BLINK, speed);
}

void EmojiController::EyeHappy() {
    PlayAnimation(AnimationType::HAPPY);
}

void EmojiController::EyeSad() {
    PlayAnimation(AnimationType::SAD);
}

void EmojiController::EyeAnger() {
    PlayAnimation(AnimationType::ANGER);
}

void EmojiController::EyeSurprise() {
    PlayAnimation(AnimationType::SURPRISE);
}

void EmojiController::EyeWakeup() {
    PlayAnimation(AnimationType::WAKEUP);
}

void EmojiController::EyeSleep() {
    PlayAnimation(AnimationType::SLEEP);
}

void EmojiController::EyeRight() {
    PlayAnimation(AnimationType::LOOK_RIGHT);
}

void EmojiController::EyeLeft() {
    PlayAnimation(AnimationType::LOOK_LEFT);
}

void EmojiController::Saccade(int direction_x, int direction_y) {
    left_eye_x_ += direction_x;
    left_eye_y_ += direction_y;
    right_eye_x_ += direction_x;
    right_eye_y_ += direction_y;

    DrawEmoji(false);
}

void EmojiController::MoveEye(int direction) {
    // direction == -1 : move left
    // direction == 1 : move right
    
    // parameters based on the original Arduino code
    int direction_oversize = 1;  // adjusted to match the deformation amplitude of the original code
    int direction_movement_amplitude = 2;  // adjusted to match the movement amplitude of the original code
    int eye_blink_amplitude = 5;
    
    // make sure the screen and eye objects exist
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "MoveEye: screen or eye object does not exist");
        return;
    }
    
    // phase 1: the eye moves in the given direction while shrinking
    for (int i = 0; i < 3; i++) {  // reduce the step count to stay consistent with the original code
        if (display_) {
            DisplayLockGuard lock(display_);
            
            left_eye_x_ += direction_movement_amplitude * direction;
            right_eye_x_ += direction_movement_amplitude * direction;
            right_eye_height_ -= eye_blink_amplitude;
            left_eye_height_ -= eye_blink_amplitude;
            
            // when looking right, the right eye grows
            if (direction > 0) {
                right_eye_height_ += direction_oversize;
                right_eye_width_ += direction_oversize;
            } else {
                // when looking left, the left eye grows
                left_eye_height_ += direction_oversize;
                left_eye_width_ += direction_oversize;
            }
            
            // update the display directly inside the lock without using DrawEmoji
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
            lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
        }
        
        // reduce the delay to shorten the lock time
        vTaskDelay(pdMS_TO_TICKS(10));  // a short delay similar to the original code
    }
    
    // phase 2: keep moving in the given direction while the eye regains height
    for (int i = 0; i < 3; i++) {  // reduce the step count to stay consistent with the original code
        if (display_) {
            DisplayLockGuard lock(display_);
            
            left_eye_x_ += direction_movement_amplitude * direction;
            right_eye_x_ += direction_movement_amplitude * direction;
            right_eye_height_ += eye_blink_amplitude;
            left_eye_height_ += eye_blink_amplitude;
            
            // when looking right, the right eye keeps growing
            if (direction > 0) {
                right_eye_height_ += direction_oversize;
                right_eye_width_ += direction_oversize;
            } else {
                // when looking left, the left eye keeps growing
                left_eye_height_ += direction_oversize;
                left_eye_width_ += direction_oversize;
            }
            
            // update the display directly inside the lock without using DrawEmoji
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
            lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
        }
        
        // reduce the delay to shorten the lock time
        vTaskDelay(pdMS_TO_TICKS(10));  // a short delay similar to the original code
    }
}

// resume the LVGL task
void EmojiController::ResumeLVGLTask() {
    if (!lvgl_task_suspended_) {
        return;
    }
    
    // find the LVGL task handle
    TaskHandle_t lvgl_task = nullptr;
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t* task_status_array = new TaskStatus_t[task_count];
    
    if (task_status_array) {
        // get all task states
        task_count = uxTaskGetSystemState(task_status_array, task_count, nullptr);
        
        // find the LVGL task
        for (UBaseType_t i = 0; i < task_count; i++) {
            if (strcmp(task_status_array[i].pcTaskName, "taskLVGL") == 0) {
                lvgl_task = task_status_array[i].xHandle;
                break;
            }
        }
        
        delete[] task_status_array;
    }
    
    // if the LVGL task is found, restore its priority
    if (lvgl_task) {
        vTaskPrioritySet(lvgl_task, saved_lvgl_task_priority_);
        lvgl_task_suspended_ = false;
        ESP_LOGI(TAG, "LVGL task priority restored");
    }
}

// helper function to safely execute animations
bool EmojiController::SafeExecuteAnimation(std::function<void()> animation_func) {
    // if an animation is already running, skip this animation
    if (is_animating_) {
        ESP_LOGW(TAG, "SafeExecuteAnimation: An animation is already running; skipping this animation");
        return false;
    }
    
    // set the animation executing flag
    is_animating_ = true;
    
    // suspend the LVGL task to reduce the refresh rate
    SuspendLVGLTask();
    
    try {
        // execute the animation function
        animation_func();
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "SafeExecuteAnimation exception: %s", e.what());
    } catch (...) {
        ESP_LOGE(TAG, "Unknown SafeExecuteAnimation exception");
    }
    
    // resume the LVGL task
    ResumeLVGLTask();
    
    // clear the animation executing flag
    is_animating_ = false;
    
    return true;
}

void EmojiController::PlayAnimation(AnimationType type, int param) {
    ESP_LOGI(TAG, "Playing animation, type: %d, parameter: %d", (int)type, param);
    
    // check whether the animation queue has been created
    if (animation_queue_ == nullptr) {
        ESP_LOGE(TAG, "PlayAnimation: animation queue has not been created");
        return;
    }
    
    // create the animation message
    AnimationMessage msg;
    msg.type = type;
    msg.param = param;
    
    // send the animation message to the queue
    if (xQueueSend(animation_queue_, &msg, 0) != pdPASS) {
        ESP_LOGW(TAG, "PlayAnimation: Failed to send animation message");
    } else {
        ESP_LOGI(TAG, "Animation message sent, type: %d", static_cast<int>(type));
    }
}

void EmojiController::StopAnimation() {
    // stop all animations
    ESP_LOGI(TAG, "Stopping all animations");
    
    // restore the eye state and update the display
    EyeCenter(true);
    
    // clear the animation queue
    ClearAnimationQueue();
    
    // clear the animation executing flag
    is_animating_ = false;
}

/**
 * @brief set whether random animation is enabled
 * @param enabled whether to enable random animation
 */
void EmojiController::SetRandomAnimationEnabled(bool enabled) {
    ESP_LOGI(TAG, "Setting random animation state: %s", enabled ? "enabled" : "disabled");
    random_animation_enabled_ = enabled;
    
    // if random animation is disabled, clear the queue so queued random animations are not executed
    if (!enabled) {
        ClearAnimationQueue();
    }
}

/**
 * @brief clear the animation queue
 */
void EmojiController::ClearAnimationQueue() {
    if (animation_queue_ == nullptr) {
        return;
    }
    
    ESP_LOGI(TAG, "Clearing animation queue");
    
    // clear all messages in the queue
    AnimationMessage msg;
    while (xQueueReceive(animation_queue_, &msg, 0) == pdPASS) {
        // just remove the message from the queue without any processing
        ESP_LOGD(TAG, "Removed animation message from queue, type: %d", (int)msg.type);
    }
}

AnimationType EmojiController::SelectRandomAnimation() {
    // generate a random number from 0-99
    uint32_t random_value = esp_random() % 100;
    
    // choose the animation type based on the probability distribution
    // blink: 60%
    // look left: 15%
    // look right: 15%
    // spin: 5%
    // puzzled: 5%
    if (random_value < 60) {
        // 60% probability: blink
        return AnimationType::BLINK;
    } else if (random_value < 78) {
        // 18% probability: look left
        return AnimationType::LOOK_LEFT;
    } else if (random_value < 96) {
        // 18% probability: look right
        return AnimationType::LOOK_RIGHT;
    } else if (random_value < 98) {
        // 2% probability: spin
        return AnimationType::HEAD_ROLL;
    } else {
        // 2% probability: puzzled
        return AnimationType::CONFUSED;
    }
}

void EmojiController::PlayRandomAnimation() {
    AnimationType type = SelectRandomAnimation();
    PlayAnimation(type);
}

void EmojiController::ExecuteRandomAnimation() {
    AnimationType type = SelectRandomAnimation();
    ESP_LOGI(TAG, "Executing random animation, type: %d", static_cast<int>(type));
    
    // check whether the screen and eye objects exist
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteRandomAnimation: screen or eye object does not exist");
        return;
    }
    
    // execute the corresponding animation based on the chosen animation type
    switch (type) {
        case AnimationType::BLINK:
            ExecuteBlinkAnimation(12);
            break;
        case AnimationType::LOOK_LEFT:
            ExecuteLookLeftAnimation();
            break;
        case AnimationType::LOOK_RIGHT:
            ExecuteLookRightAnimation();
            break;
        case AnimationType::HEAD_NOD:
            ExecuteHeadNodAnimation();
            break;
        case AnimationType::HAPPY:
            ExecuteHappyAnimation();
            break;
        case AnimationType::HEAD_ROLL:
            ExecuteHeadRollAnimation();
            break;
        case AnimationType::SAD:
            ExecuteSadAnimation();
            break;
        case AnimationType::ANGER:
            ExecuteAngerAnimation();
            break;
        case AnimationType::SURPRISE:
            ExecuteSurpriseAnimation();
            break;
        case AnimationType::CONFUSED:
            ExecuteConfusedAnimation();
            break;
        case AnimationType::AWKWARD:
            ExecuteAwkwardAnimation();
            break;
        default:
            ExecuteBlinkAnimation(12);
            break;
    }
}

void EmojiController::ExecuteBlinkAnimation(int speed) {
    ESP_LOGI(TAG, "Executing blink animation, speed: %d", speed);
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteBlinkAnimation: screen or eye object does not exist");
        return;
    }
    
    // check whether there is a servo controller
    bool has_servo = (servo_controller_ != nullptr);
    
    // save the original height
    int original_left_height_ = left_eye_height_;
    int original_right_height_ = right_eye_height_;
    
    // blink animation: the eye height gradually decreases from its current value to 0, then restores
    const int steps = 8;  // increase the step count to make the animation smoother
    int left_step = original_left_height_ / steps;
    int right_step = original_right_height_ / steps;
    
    // make sure the step is at least 1
    left_step = left_step > 0 ? left_step : 1;
    right_step = right_step > 0 ? right_step : 1;
    
    // compute the delay; the higher the speed, the smaller the delay
    int delay_ms = 200 / speed;  // increase the base delay
    if (delay_ms < 10) delay_ms = 10; // make sure of the minimum delay
    
    // randomly decide whether to blink once or twice in a row
    bool double_blink = (esp_random() % 100) < 40;  // 40% chance of blinking twice in a row
    int blink_count = double_blink ? 2 : 1;
    
    // randomly decide whether to add a slight head movement - only a 30% chance
    bool add_head_movement = has_servo && ((esp_random() % 100) < 30);
    int head_direction = 0; // 0=none, 1=slightly down-left, 2=slightly up-right, 3=slightly up, 4=slightly down, 5=slightly up-left, 6=slightly down-right
    
    if (add_head_movement) {
        // pick a random direction, avoiding the modulo operation to reduce computation
        uint32_t rand_val = esp_random() & 0x07; // take the lowest 3 bits, value 0-7
        head_direction = (rand_val % 6) + 1; // 1-6, make sure the range is correct
        ESP_LOGI(TAG, "Adding head movement during blink, direction: %d", head_direction);
    }
    
    ESP_LOGI(TAG, "Blink count: %d", blink_count);
    
    // use a lock to protect the entire blink process
    if (display_) {
        try {
            // perform one or two blinks
            for (int blink = 0; blink < blink_count; blink++) {
                // eye closing phase
                for (int i = 0; i < steps; i++) {
                    DisplayLockGuard lock(display_);
                    
                    left_eye_height_ -= left_step;
                    right_eye_height_ -= right_step;
                    
                    // make sure the height does not become negative
                    if (left_eye_height_ < 0) left_eye_height_ = 0;
                    if (right_eye_height_ < 0) right_eye_height_ = 0;
                    
                    // update the eye objects directly
                    lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
                    lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
                    lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
                    lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
                    
                    // short delay after each animation step to make the animation more noticeable
                    vTaskDelay(pdMS_TO_TICKS(delay_ms));
                }
                
                // if a head movement is needed
                if (has_servo && blink == 0) {
                    // perform a slight head movement
                    const int small_angle = 25; // increased to 20 degrees so the motion is more noticeable but still natural
                    // const int delay_param = 30; // servo movement delay parameter
                    
                    // use the HeadMove method for all head movements
                    switch (head_direction) {
                        case 1: // slightly down-left
                            servo_controller_->HeadMove(-small_angle, -small_angle, SERVO_DELAY);
                            break;
                        case 2: // slightly up-right
                            servo_controller_->HeadMove(small_angle, small_angle, SERVO_DELAY);
                            break;
                        case 3: // slightly up
                            servo_controller_->HeadMove(0, small_angle, SERVO_DELAY);
                            break;
                        case 4: // slightly down
                            servo_controller_->HeadMove(0, -small_angle, SERVO_DELAY);
                            break;
                        case 5: // slightly up-left
                            servo_controller_->HeadMove(-small_angle, small_angle, SERVO_DELAY);
                            break;
                        case 6: // slightly down-right
                            servo_controller_->HeadMove(small_angle, -small_angle, SERVO_DELAY);
                            break;
                            
                    }
                }
                
                // briefly hold the eyes closed
                // for a double blink, keep the eyes closed for less time after the first blink
                int closed_delay = double_blink && blink == 0 ? delay_ms : delay_ms * 3;
                vTaskDelay(pdMS_TO_TICKS(closed_delay));
                
                // eye opening phase
                for (int i = 0; i < steps; i++) {
                    DisplayLockGuard lock(display_);
                    
                    left_eye_height_ += left_step;
                    right_eye_height_ += right_step;
                    
                    // make sure it does not exceed the original height
                    if (left_eye_height_ > original_left_height_) left_eye_height_ = original_left_height_;
                    if (right_eye_height_ > original_right_height_) right_eye_height_ = original_right_height_;
                    
                    // update the eye objects directly
                    lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
                    lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
                    lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
                    lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
                    
                    // short delay after each animation step to make the animation more noticeable
                    vTaskDelay(pdMS_TO_TICKS(delay_ms));
                }
                
                // for a double blink, pause briefly between the two blinks
                if (double_blink && blink == 0) {
                    vTaskDelay(pdMS_TO_TICKS(delay_ms * 2));
                }
                
                // after the last blink completes, restore the head position if there was head movement
                if (add_head_movement && has_servo && blink == blink_count - 1) {
                    servo_controller_->HeadCenter(10); // restore the head position using delay parameter 10
                }
            }
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteBlinkAnimation: Failed to acquire display lock");
        }
    }
    
    // make sure the eye height returns to its original value
    left_eye_height_ = original_left_height_;
    right_eye_height_ = original_right_height_;
}

void EmojiController::ExecuteHappyAnimation() {
    ESP_LOGI(TAG, "Executing happy expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteHappyAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // create the happy expression - simplified version with fewer iterations and delay
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // get the eye position and size
            int offset = left_eye_height_ / 2;
            
            // create the left and right eye triangle objects
            lv_obj_t* left_eye_mask = nullptr;
            lv_obj_t* right_eye_mask = nullptr;
            
            // reduce the loop count from 10 to 5
            for (int i = 0; i < 5; i++) {
                // if the triangle objects were already created, delete them first
                if (left_eye_mask) lv_obj_del(left_eye_mask);
                if (right_eye_mask) lv_obj_del(right_eye_mask);
                
                // create the left eye triangle
                left_eye_mask = lv_obj_create(emoji_screen_);
                lv_obj_set_style_bg_color(left_eye_mask, lv_color_white(), 0); // set to white
                lv_obj_set_style_border_width(left_eye_mask, 0, 0);
                lv_obj_set_style_radius(left_eye_mask, 0, 0); // set to a right angle
                
                // create a triangle-shaped object
                // use three points to define the triangle region
                int x1 = left_eye_x_ - left_eye_width_/2 - 5;
                int y1 = left_eye_y_ + offset - 3;
                int x2 = left_eye_x_ + left_eye_width_/2 + 5;
                // the y2 variable is already used, keep it
                // the x3 variable is already used, keep it
                int y3 = left_eye_y_ + left_eye_height_ + offset + 3;
                
                // set the triangle size and position - approximated with a rectangle
                int tri_width = x2 - x1 + 15;  // increase the width
                int tri_height = y3 - y1 + 15;  // increase the height
                
                lv_obj_set_size(left_eye_mask, tri_width, tri_height);
                lv_obj_set_pos(left_eye_mask, x1 - 8, y1 - 8);
                
                // create a triangle effect using the CSS clip-path property
                // but LVGL may not support clip-path, so we use a simple workaround
                // set the rotation and tilt of the left eye triangle
                lv_obj_set_style_transform_angle(left_eye_mask, 80, 0); // 8 degrees, in units of 0.1 degree
                lv_obj_set_style_transform_pivot_x(left_eye_mask, 0, 0);
                lv_obj_set_style_transform_pivot_y(left_eye_mask, 0, 0);
                
                // create the right eye triangle
                right_eye_mask = lv_obj_create(emoji_screen_);
                lv_obj_set_style_bg_color(right_eye_mask, lv_color_white(), 0); // set to white
                lv_obj_set_style_border_width(right_eye_mask, 0, 0);
                lv_obj_set_style_radius(right_eye_mask, 0, 0); // set to a right angle
                
                // use three points to define the triangle region
                int rx1 = right_eye_x_ + right_eye_width_/2 + 5;
                int ry1 = right_eye_y_ + offset - 3;
                int rx2 = right_eye_x_ - right_eye_width_/2 - 5;
                // the ry2 variable is already used, keep it
                // the rx3 variable is already used, keep it
                int ry3 = right_eye_y_ + right_eye_height_ + offset + 3;
                
                // set the triangle size and position - approximated with a rectangle
                int rtri_width = rx1 - rx2 + 15;  // increase the width
                int rtri_height = ry3 - ry1 + 15;  // increase the height
                
                lv_obj_set_size(right_eye_mask, rtri_width, rtri_height);
                lv_obj_set_pos(right_eye_mask, rx2 - 8, ry1 - 8);
                
                // set the rotation and tilt of the right eye triangle
                lv_obj_set_style_transform_angle(right_eye_mask, -80, 0); // -8 degrees, in units of 0.1 degree
                lv_obj_set_style_transform_pivot_x(right_eye_mask, rtri_width, 0);
                lv_obj_set_style_transform_pivot_y(right_eye_mask, 0, 0);
                
                // update the offset to match the reference code
                offset -= 2;
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay to create the animation effect, with reduced delay
                vTaskDelay(pdMS_TO_TICKS(10)); // reduced from 20ms to 10ms
            }
            
            // if needed, tilt the servo slightly up to show happiness
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadMove(0, -15, 10);  // a negative y offset makes the servo move upward
            }
            
            // display for a while, with reduced duration
            vTaskDelay(pdMS_TO_TICKS(1000)); // reduced from 2000ms to 1000ms
            
            // clean up resources
            if (left_eye_mask) lv_obj_del(left_eye_mask);
            if (right_eye_mask) lv_obj_del(right_eye_mask);
            
            // restore the eye state
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // refresh the display
            lv_refr_now(NULL);
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "ExecuteHappyAnimation exception: %s", e.what());
        }
    } else {
        ESP_LOGW(TAG, "ExecuteHappyAnimation: display object does not exist");
    }
}

void EmojiController::ExecuteSadAnimation() {
    ESP_LOGI(TAG, "Executing sad expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteSadAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // create the sad expression - using triangles
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // get the eye position and size
            int offset = left_eye_height_ / 4;  // reduce the initial offset
            
            // create the left and right eye triangle objects
            lv_obj_t* left_eye_mask = nullptr;
            lv_obj_t* right_eye_mask = nullptr;
            
            // reference the implementation in boardemoji.ino
            // draw multiple triangles in a loop, gradually shifting position to create the animation
            for (int i = 0; i < 10; i++) {
                // if the triangle objects were already created, delete them first
                if (left_eye_mask) lv_obj_del(left_eye_mask);
                if (right_eye_mask) lv_obj_del(right_eye_mask);
                
                // create the left eye triangle
                left_eye_mask = lv_obj_create(emoji_screen_);
                lv_obj_set_style_bg_color(left_eye_mask, lv_color_white(), 0); // set to white
                lv_obj_set_style_border_width(left_eye_mask, 0, 0);
                lv_obj_set_style_radius(left_eye_mask, 0, 0); // set to a right angle
                
                // compute the triangle coordinates of the happy expression
                int happy_x1 = left_eye_x_ - left_eye_width_/2 - 5;
                int happy_y1 = left_eye_y_ + offset - 3;
                int happy_x2 = left_eye_x_ + left_eye_width_/2 + 5;
                int happy_y2 = left_eye_y_ + 5 + offset;
                int happy_x3 = left_eye_x_ - left_eye_width_/2 - 5;
                int happy_y3 = left_eye_y_ + left_eye_height_ + offset + 3;
                
                // compute the triangle coordinates of the sad expression (vertically flipped)
                int sad_x1 = happy_x1;
                int sad_y1 = 2 * left_eye_y_ - happy_y1;
                int sad_x2 = happy_x2;
                // int sad_y2 = 2 \* left_eye_y_ - happy_y2;
                // int sad_x3 = happy_x3;
                int sad_y3 = 2 * left_eye_y_ - happy_y3;
                
                // set the triangle size and position
                int tri_width = sad_x2 - sad_x1 + 15;
                int tri_height = abs(sad_y3 - sad_y1) + 15;
                
                // determine the top-left coordinate (take the smallest x and y)
                int top_left_x = sad_x1 - 8;
                int top_left_y = std::min(sad_y1, sad_y3) - 15;  
                
                lv_obj_set_size(left_eye_mask, tri_width, tri_height);
                lv_obj_set_pos(left_eye_mask, top_left_x, top_left_y);
                
                // set the rotation and tilt of the left eye triangle
                lv_obj_set_style_transform_angle(left_eye_mask, -100, 0); // -10 degrees, in units of 0.1 degree
                // set the rotation center to the triangle's bottom-left corner
                lv_obj_set_style_transform_pivot_x(left_eye_mask, 0, 0);
                lv_obj_set_style_transform_pivot_y(left_eye_mask, tri_height, 0);
                
                // create the right eye triangle
                right_eye_mask = lv_obj_create(emoji_screen_);
                lv_obj_set_style_bg_color(right_eye_mask, lv_color_white(), 0); // set to white
                lv_obj_set_style_border_width(right_eye_mask, 0, 0);
                lv_obj_set_style_radius(right_eye_mask, 0, 0); // set to a right angle
                
                // compute the triangle coordinates of the happy expression
                int happy_rx1 = right_eye_x_ + right_eye_width_/2 + 5;
                int happy_ry1 = right_eye_y_ + offset - 3;
                int happy_rx2 = right_eye_x_ - right_eye_width_/2 - 5;
                int happy_ry2 = right_eye_y_ + 5 + offset;
                int happy_rx3 = right_eye_x_ + right_eye_width_/2 + 5;
                int happy_ry3 = right_eye_y_ + right_eye_height_ + offset + 3;
                
                // compute the triangle coordinates of the sad expression (vertically flipped)
                int sad_rx1 = happy_rx1;
                int sad_ry1 = 2 * right_eye_y_ - happy_ry1;
                int sad_rx2 = happy_rx2;
                // int sad_ry2 = 2 \* right_eye_y_ - happy_ry2;
                // int sad_rx3 = happy_rx3;
                int sad_ry3 = 2 * right_eye_y_ - happy_ry3;
                
                // set the triangle size and position
                int rtri_width = sad_rx1 - sad_rx2 + 15;
                int rtri_height = abs(sad_ry3 - sad_ry1) + 15;
                
                // determine the top-left coordinate (take the smallest x and y)
                int rtop_left_x = sad_rx2 - 8;
                int rtop_left_y = std::min(sad_ry1, sad_ry3) - 15;  
                
                lv_obj_set_size(right_eye_mask, rtri_width, rtri_height);
                lv_obj_set_pos(right_eye_mask, rtop_left_x, rtop_left_y);
                
                // set the rotation and tilt of the right eye triangle
                lv_obj_set_style_transform_angle(right_eye_mask, 100, 0); // 10 degrees, in units of 0.1 degree
                // set the rotation center to the triangle's bottom-right corner
                lv_obj_set_style_transform_pivot_x(right_eye_mask, rtri_width, 0);
                lv_obj_set_style_transform_pivot_y(right_eye_mask, rtri_height, 0);
                
                // update the offset to match the reference code
                offset -= 2;
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            
            // if needed, tilt the servo slightly down to show sadness
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadMove(0, 20, 1);  // nod down
                servo_controller_->HeadMove(0, -20, 1);  // tilt up
            }
            
            // display for a while
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            // delete the created objects
            if (left_eye_mask) lv_obj_del(left_eye_mask);
            if (right_eye_mask) lv_obj_del(right_eye_mask);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteSadAnimation: Failed to acquire display lock");
        }
    }
    
    // add a short delay to make sure the display update completes
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
    
    // restore the original state
    EyeCenter();
}

void EmojiController::ExecuteLaughingAnimation() {
    ESP_LOGI(TAG, "Executing laughing expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteLaughingAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size
            const int original_left_width_ = left_eye_width_;
            const int original_left_height_ = left_eye_height_;
            const int original_right_width_ = right_eye_width_;
            const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // laugh expression - the eyes become crescent-shaped (by shrinking the height and rotating)
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the animation effect - the eye height shrinks and moves up
            const int animation_steps = 8;
            const int animation_delay_ms = 10;
            const float min_height_factor = 0.3f; // minimum height is 30% of the original height
            
            // eye shrinking animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float height_factor = 1.0f - progress * (1.0f - min_height_factor);
                
                // compute the current size
                int current_left_height = original_left_height_ * height_factor;
                int current_right_height = original_right_height_ * height_factor;
                
                // update the eye height and position (moving up)
                lv_obj_set_height(left_eye_, current_left_height);
                lv_obj_set_height(right_eye_, current_right_height);
                
                // the eye moves up while keeping the bottom position fixed
                int y_offset = (original_left_height_ - current_left_height) / 2;
                lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, 
                              left_eye_y_ - current_left_height/2 - y_offset);
                lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, 
                              right_eye_y_ - current_right_height/2 - y_offset);
                
                // set the corner radius - increase it for laughing
                int current_radius = original_corner_radius_ + (int)(progress * 10);
                lv_obj_set_style_radius(left_eye_, current_radius, 0);
                lv_obj_set_style_radius(right_eye_, current_radius, 0);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head back to show laughing
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadUp(15); // a larger tilt-up angle to show laughing
            }
            
            // add a shake effect to mimic trembling from laughing
            const int shake_steps = 5;
            const int shake_delay_ms = 50;
            const int shake_offset = 2;
            
            for (int i = 0; i < shake_steps; i++) {
                // small left-right shake
                int offset_x = (i % 2 == 0) ? shake_offset : -shake_offset;
                
                // update the position
                lv_obj_set_pos(left_eye_, 
                              left_eye_x_ - left_eye_width_/2 + offset_x, 
                              lv_obj_get_y(left_eye_));
                lv_obj_set_pos(right_eye_, 
                              right_eye_x_ - right_eye_width_/2 + offset_x, 
                              lv_obj_get_y(right_eye_));
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(shake_delay_ms));
            }
            
            // hold the laughing state for a while
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            lv_obj_set_style_radius(left_eye_, original_corner_radius_, 0);
            lv_obj_set_style_radius(right_eye_, original_corner_radius_, 0);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteLaughingAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteConfidentAnimation() {
    ESP_LOGI(TAG, "Executing confident expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteConfidentAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // confident expression - the eyes narrow and the head tilts slightly up
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the confident animation effect - the eyes narrow
            const int animation_steps = 8;
            const int animation_delay_ms = 15;
            const float min_height_factor = 0.5f; // minimum height is 50% of the original height
            
            // eye narrowing animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float height_factor = 1.0f - progress * (1.0f - min_height_factor);
                
                // compute the current size
                int current_left_height = original_left_height_ * height_factor;
                int current_right_height = original_right_height_ * height_factor;
                
                // update the eye height and position
                lv_obj_set_height(left_eye_, current_left_height);
                lv_obj_set_height(right_eye_, current_right_height);
                
                lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, 
                              left_eye_y_ - current_left_height/2);
                lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, 
                              right_eye_y_ - current_right_height/2);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head up
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadUp(10); // tilt up to show confidence
            }
            
            // add the effect of the eyes looking to the right
            EyeRight();
            vTaskDelay(pdMS_TO_TICKS(300));
            
            // add a slight head-nod effect
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadNod(5); // small head nod
            }
            
            // hold the confident state for a while
            vTaskDelay(pdMS_TO_TICKS(800));
            
            // restore the original state
            lv_obj_set_height(left_eye_, original_left_height_);
            lv_obj_set_height(right_eye_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteConfidentAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo and eye positions
    EyeCenter(true);
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteSillyAnimation() {
    ESP_LOGI(TAG, "Executing sly expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteSillyAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // smirk expression - one eye shrinks while the other stays normal
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the smirk animation effect - the left eye shrinks
            const int animation_steps = 8;
            const int animation_delay_ms = 15;
            const float min_scale = 0.4f; // minimum scale factor
            
            // left eye shrinking animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float scale = 1.0f - progress * (1.0f - min_scale);
                
                // compute the current size
                int current_width = original_left_width_ * scale;
                int current_height = original_left_height_ * scale;
                
                // shrink only the left eye
                lv_obj_set_size(left_eye_, current_width, current_height);
                lv_obj_set_pos(left_eye_, left_eye_x_ - current_width/2, left_eye_y_ - current_height/2);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head sideways
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadMove(10, 0, 10); // tilt right
            }
            
            // add the effect of the eyes moving left and right
            const int look_steps = 2;
            const int look_delay_ms = 300;
            
            for (int i = 0; i < look_steps; i++) {
                // look left
                EyeLeft();
                vTaskDelay(pdMS_TO_TICKS(look_delay_ms));
                
                // look right
                EyeRight();
                vTaskDelay(pdMS_TO_TICKS(look_delay_ms));
            }
            
            // add a slight head-sway effect
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadShake(5); // small head sway
            }
            
            // restore the center position
            EyeCenter(true);
            vTaskDelay(pdMS_TO_TICKS(200));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteSillyAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteDeliciousAnimation() {
    ESP_LOGI(TAG, "Executing delicious expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteDeliciousAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            
            // yummy expression - the eyes shrink and move slightly up
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the yummy animation effect - the eyes shrink and move up
            const int animation_steps = 8;
            const int animation_delay_ms = 15;
            const float min_scale = 0.7f; // minimum scale factor
            const int max_y_offset = -3;  // maximum Y-axis offset (negative is up)
            
            // eye shrinking and moving-up animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float scale = 1.0f - progress * (1.0f - min_scale);
                int y_offset = progress * max_y_offset;
                
                // compute the current size
                int current_left_width = original_left_width_ * scale;
                int current_left_height = original_left_height_ * scale;
                int current_right_width = original_right_width_ * scale;
                int current_right_height = original_right_height_ * scale;
                
                // update the eye size and position
                lv_obj_set_size(left_eye_, current_left_width, current_left_height);
                lv_obj_set_size(right_eye_, current_right_width, current_right_height);
                
                lv_obj_set_pos(left_eye_, 
                              left_eye_x_ - current_left_width/2, 
                              left_eye_y_ - current_left_height/2 + y_offset);
                lv_obj_set_pos(right_eye_, 
                              right_eye_x_ - current_right_width/2, 
                              right_eye_y_ - current_right_height/2 + y_offset);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, make the head nod slightly
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadNod(5); // small head nod
            }
            
            // add a small left-right servo sway to mimic tasting food
            if (servo_controller_ != nullptr) {
                // small left-right sway
                servo_controller_->HeadMove(5, 0, 10);
                vTaskDelay(pdMS_TO_TICKS(300));
                servo_controller_->HeadMove(-5, 0, 10);
                vTaskDelay(pdMS_TO_TICKS(300));
                servo_controller_->HeadMove(5, 0, 10);
            }
            
            // hold the yummy state for a while
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteDeliciousAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteKissyAnimation() {
    ESP_LOGI(TAG, "Executing kissing expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteKissyAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // kiss expression - the eyes shrink and become round
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the kiss animation effect - the eyes shrink and become round
            const int animation_steps = 10;
            const int animation_delay_ms = 15;
            const float min_scale = 0.5f; // minimum scale factor
            
            // eye shrinking and rounding animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float scale = 1.0f - progress * (1.0f - min_scale);
                
                // compute the current size
                int current_width = original_left_width_ * scale;
                int current_height = original_left_height_ * scale;
                
                // update the eye size and position
                lv_obj_set_size(left_eye_, current_width, current_height);
                lv_obj_set_size(right_eye_, current_width, current_height);
                
                lv_obj_set_pos(left_eye_, left_eye_x_ - current_width/2, left_eye_y_ - current_height/2);
                lv_obj_set_pos(right_eye_, right_eye_x_ - current_width/2, right_eye_y_ - current_height/2);
                
                // set the eyes to round
                int current_radius = original_corner_radius_ + (int)(progress * (current_width/2 - original_corner_radius_));
                lv_obj_set_style_radius(left_eye_, current_radius, 0);
                lv_obj_set_style_radius(right_eye_, current_radius, 0);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head slightly forward
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadMove(0, -10, 10); // tilt the head forward
            }
            
            // add a mouth movement (simulated via eye control)
            const int kiss_steps = 3;
            const int kiss_delay_ms = 200;
            
            for (int i = 0; i < kiss_steps; i++) {
                // mouth shrinks (the eyes become smaller)
                float pucker_scale = 0.3f;
                int pucker_width = original_left_width_ * pucker_scale;
                int pucker_height = original_left_height_ * pucker_scale;
                
                lv_obj_set_size(left_eye_, pucker_width, pucker_height);
                lv_obj_set_size(right_eye_, pucker_width, pucker_height);
                
                lv_obj_set_pos(left_eye_, left_eye_x_ - pucker_width/2, left_eye_y_ - pucker_height/2);
                lv_obj_set_pos(right_eye_, right_eye_x_ - pucker_width/2, right_eye_y_ - pucker_height/2);
                
                // fully round
                lv_obj_set_style_radius(left_eye_, pucker_width/2, 0);
                lv_obj_set_style_radius(right_eye_, pucker_width/2, 0);
                
                lv_refr_now(NULL);
                vTaskDelay(pdMS_TO_TICKS(kiss_delay_ms));
                
                // the mouth recovers a little
                float relax_scale = 0.5f;
                int relax_width = original_left_width_ * relax_scale;
                int relax_height = original_left_height_ * relax_scale;
                
                lv_obj_set_size(left_eye_, relax_width, relax_height);
                lv_obj_set_size(right_eye_, relax_width, relax_height);
                
                lv_obj_set_pos(left_eye_, left_eye_x_ - relax_width/2, left_eye_y_ - relax_height/2);
                lv_obj_set_pos(right_eye_, right_eye_x_ - relax_width/2, right_eye_y_ - relax_height/2);
                
                // round eyes
                lv_obj_set_style_radius(left_eye_, relax_width/2, 0);
                lv_obj_set_style_radius(right_eye_, relax_width/2, 0);
                
                lv_refr_now(NULL);
                vTaskDelay(pdMS_TO_TICKS(kiss_delay_ms));
            }
            
            // hold the kiss state for a while
            vTaskDelay(pdMS_TO_TICKS(300));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            lv_obj_set_style_radius(left_eye_, original_corner_radius_, 0);
            lv_obj_set_style_radius(right_eye_, original_corner_radius_, 0);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteKissyAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteCoolAnimation() {
    ESP_LOGI(TAG, "Executing cool expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteCoolAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // cool expression - the eyes shrink and narrow
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the cool animation effect - the eyes narrow
            const int animation_steps = 8;
            const int animation_delay_ms = 15;
            const float min_height_factor = 0.4f; // minimum height is 40% of the original height
            
            // eye narrowing animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float height_factor = 1.0f - progress * (1.0f - min_height_factor);
                
                // compute the current size
                int current_left_height = original_left_height_ * height_factor;
                int current_right_height = original_right_height_ * height_factor;
                
                // update the eye height and position
                lv_obj_set_height(left_eye_, current_left_height);
                lv_obj_set_height(right_eye_, current_right_height);
                
                lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, 
                              left_eye_y_ - current_left_height/2);
                lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, 
                              right_eye_y_ - current_right_height/2);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head slightly up
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadUp(5); // tilt slightly up
            }
            
            // add the effect of the eyes looking to the right
            EyeRight();
            vTaskDelay(pdMS_TO_TICKS(300));
            
            // add a slight head-sway effect
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadNod(5); // small head nod
            }
            
            // hold the cool state for a while
            vTaskDelay(pdMS_TO_TICKS(800));
            
            // restore the original state
            lv_obj_set_height(left_eye_, original_left_height_);
            lv_obj_set_height(right_eye_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteCoolAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo and eye positions
    EyeCenter(true);
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteRelaxedAnimation() {
    ESP_LOGI(TAG, "Executing relaxed expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteRelaxedAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            
            // relaxed expression - the eyes shrink and narrow, like being half asleep
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the relaxed animation effect - the eyes shrink and narrow
            const int animation_steps = 10;
            const int animation_delay_ms = 20;
            const float min_height_factor = 0.5f; // minimum height is 50% of the original height
            const float min_width_factor = 0.9f;  // minimum width is 90% of the original width
            
            // eye shrinking and narrowing animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float height_factor = 1.0f - progress * (1.0f - min_height_factor);
                float width_factor = 1.0f - progress * (1.0f - min_width_factor);
                
                // compute the current size
                int current_left_width = original_left_width_ * width_factor;
                int current_left_height = original_left_height_ * height_factor;
                int current_right_width = original_right_width_ * width_factor;
                int current_right_height = original_right_height_ * height_factor;
                
                // update the eye size and position
                lv_obj_set_size(left_eye_, current_left_width, current_left_height);
                lv_obj_set_size(right_eye_, current_right_width, current_right_height);
                
                lv_obj_set_pos(left_eye_, left_eye_x_ - current_left_width/2, 
                              left_eye_y_ - current_left_height/2);
                lv_obj_set_pos(right_eye_, right_eye_x_ - current_right_width/2, 
                              right_eye_y_ - current_right_height/2);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head slightly down
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadDown(5); // tilt slightly down
            }
            
            // add a slow eye blink
            ExecuteBlinkAnimation(2); // slow blink
            
            // hold the relaxed state for a while
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteRelaxedAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteShockedAnimation() {
    ESP_LOGI(TAG, "Executing shocked expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteShockedAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // shocked expression - the eyes suddenly enlarge and become round
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the shocked animation effect - the eyes suddenly enlarge
            const int animation_steps = 5;
            const int animation_delay_ms = 10;
            const float max_scale = 1.5f; // maximum scale factor
            
            // eye sudden-enlarging animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float scale = 1.0f + progress * (max_scale - 1.0f);
                
                // compute the current size
                int current_width = original_left_width_ * scale;
                int current_height = original_left_height_ * scale;
                
                // update the eye size and position
                lv_obj_set_size(left_eye_, current_width, current_height);
                lv_obj_set_size(right_eye_, current_width, current_height);
                
                lv_obj_set_pos(left_eye_, left_eye_x_ - current_width/2, left_eye_y_ - current_height/2);
                lv_obj_set_pos(right_eye_, right_eye_x_ - current_width/2, right_eye_y_ - current_height/2);
                
                // fully round eyes
                lv_obj_set_style_radius(left_eye_, current_width/2, 0);
                lv_obj_set_style_radius(right_eye_, current_width/2, 0);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head back to show shock
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadUp(15); // tilt the head back
            }
            
            // add a slight vibration effect
            const int shake_steps = 8;
            const int shake_delay_ms = 30;
            const int shake_offset = 1;
            
            for (int i = 0; i < shake_steps; i++) {
                // small shake in a random direction
                int offset_x = (i % 2 == 0) ? shake_offset : -shake_offset;
                int offset_y = ((i / 2) % 2 == 0) ? shake_offset : -shake_offset;
                
                // update the position
                lv_obj_set_pos(left_eye_, 
                              left_eye_x_ - lv_obj_get_width(left_eye_)/2 + offset_x, 
                              left_eye_y_ - lv_obj_get_height(left_eye_)/2 + offset_y);
                lv_obj_set_pos(right_eye_, 
                              right_eye_x_ - lv_obj_get_width(right_eye_)/2 + offset_x, 
                              right_eye_y_ - lv_obj_get_height(right_eye_)/2 + offset_y);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(shake_delay_ms));
            }
            
            // hold the shocked state for a while
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            lv_obj_set_style_radius(left_eye_, original_corner_radius_, 0);
            lv_obj_set_style_radius(right_eye_, original_corner_radius_, 0);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteShockedAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteThinkingAnimation() {
    ESP_LOGI(TAG, "Executing thinking expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteThinkingAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // thinking expression - one eye shrinks while the other stays normal
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the thinking animation effect - the right eye shrinks
            const int animation_steps = 8;
            const int animation_delay_ms = 20;
            const float min_scale = 0.3f; // minimum scale factor
            
            // right eye shrinking animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float scale = 1.0f - progress * (1.0f - min_scale);
                
                // compute the current size
                int current_width = original_right_width_ * scale;
                int current_height = original_right_height_ * scale;
                
                // shrink only the right eye
                lv_obj_set_size(right_eye_, current_width, current_height);
                lv_obj_set_pos(right_eye_, right_eye_x_ - current_width/2, right_eye_y_ - current_height/2);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head sideways
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadMove(-10, 5, 10); // tilt left, slightly up
            }
            
            // add the effect of the eyes looking up
            EyeUp();
            vTaskDelay(pdMS_TO_TICKS(300));
            
            // add the effect of the eyes moving left and right to simulate thinking
            const int look_steps = 3;
            const int look_delay_ms = 500;
            
            for (int i = 0; i < look_steps; i++) {
                // look left
                EyeLeft();
                vTaskDelay(pdMS_TO_TICKS(look_delay_ms));
                
                // look right
                EyeRight();
                vTaskDelay(pdMS_TO_TICKS(look_delay_ms));
            }
            
            // restore the center position
            EyeCenter(true);
            vTaskDelay(pdMS_TO_TICKS(200));
            
            // restore the original state
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteThinkingAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteLovingAnimation() {
    ESP_LOGI(TAG, "Executing loving expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteLovingAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size
            const int original_left_width_ = left_eye_width_;
            const int original_left_height_ = left_eye_height_;
            const int original_right_width_ = right_eye_width_;
            const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // heart expression - the eyes become heart-shaped
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the heart-shaped eye animation
            const int animation_steps = 10;
            const int animation_delay_ms = 20;
            
            // heart-shape morphing animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                
                // compute the current size - the heart is slightly wider than tall
                int heart_width = original_left_width_ * (0.8f + progress * 0.2f);
                int heart_height = original_left_height_ * (0.7f + progress * 0.3f);
                
                // update the eye size
                lv_obj_set_size(left_eye_, heart_width, heart_height);
                lv_obj_set_size(right_eye_, heart_width, heart_height);
                
                // update the eye position
                lv_obj_set_pos(left_eye_, left_eye_x_ - heart_width/2, left_eye_y_ - heart_height/2);
                lv_obj_set_pos(right_eye_, right_eye_x_ - heart_width/2, right_eye_y_ - heart_height/2);
                
                // heart effect - achieved with corner radii and shape
                // the heart is pointed at the top and rounded at the bottom
                int heart_radius = original_corner_radius_ + (int)(progress * 15);
                lv_obj_set_style_radius(left_eye_, heart_radius, 0);
                lv_obj_set_style_radius(right_eye_, heart_radius, 0);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // add a heartbeat effect
            const int pulse_steps = 4;
            const int pulse_cycles = 3;
            const int pulse_delay_ms = 100;
            const float max_scale = 1.2f;
            
            for (int cycle = 0; cycle < pulse_cycles; cycle++) {
                // heartbeat expands
                for (int step = 0; step < pulse_steps; step++) {
                    float progress = (float)step / pulse_steps;
                    float scale = 1.0f + progress * (max_scale - 1.0f);
                    
                    int current_width = original_left_width_ * scale;
                    int current_height = original_left_height_ * scale;
                    
                    lv_obj_set_size(left_eye_, current_width, current_height);
                    lv_obj_set_size(right_eye_, current_width, current_height);
                    
                    lv_obj_set_pos(left_eye_, left_eye_x_ - current_width/2, left_eye_y_ - current_height/2);
                    lv_obj_set_pos(right_eye_, right_eye_x_ - current_width/2, right_eye_y_ - current_height/2);
                    
                    lv_refr_now(NULL);
                    vTaskDelay(pdMS_TO_TICKS(pulse_delay_ms));
                }
                
                // heartbeat shrinks
                for (int step = 0; step < pulse_steps; step++) {
                    float progress = (float)step / pulse_steps;
                    float scale = max_scale - progress * (max_scale - 1.0f);
                    
                    int current_width = original_left_width_ * scale;
                    int current_height = original_left_height_ * scale;
                    
                    lv_obj_set_size(left_eye_, current_width, current_height);
                    lv_obj_set_size(right_eye_, current_width, current_height);
                    
                    lv_obj_set_pos(left_eye_, left_eye_x_ - current_width/2, left_eye_y_ - current_height/2);
                    lv_obj_set_pos(right_eye_, right_eye_x_ - current_width/2, right_eye_y_ - current_height/2);
                    
                    lv_refr_now(NULL);
                    vTaskDelay(pdMS_TO_TICKS(pulse_delay_ms));
                }
            }
            
            // if there is a servo controller, make it sway slightly
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadRoll(10); // small head sway
            }
            
            // hold the heart state for a while
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            lv_obj_set_style_radius(left_eye_, original_corner_radius_, 0);
            lv_obj_set_style_radius(right_eye_, original_corner_radius_, 0);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteLovingAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteEmbarrassedAnimation() {
    ESP_LOGI(TAG, "Executing embarrassed expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteEmbarrassedAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            const int original_left_x = left_eye_x_;
            const int original_left_y = left_eye_y_;
            const int original_right_x = right_eye_x_;
            const int original_right_y = right_eye_y_;
            
            // embarrassed expression - the eyes move slightly down and shrink
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the animation effect
            const int animation_steps = 8;
            const int animation_delay_ms = 15;
            const float min_scale = 0.7f; // minimum scale factor
            const int max_y_offset = 5;   // maximum Y-axis offset
            
            // eye shrinking and moving-down animation
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                float scale = 1.0f - progress * (1.0f - min_scale);
                int y_offset = progress * max_y_offset;
                
                // compute the current size
                int current_left_width = original_left_width_ * scale;
                int current_left_height = original_left_height_ * scale;
                int current_right_width = original_right_width_ * scale;
                int current_right_height = original_right_height_ * scale;
                
                // update the eye size and position
                lv_obj_set_size(left_eye_, current_left_width, current_left_height);
                lv_obj_set_size(right_eye_, current_right_width, current_right_height);
                
                lv_obj_set_pos(left_eye_, 
                              original_left_x - current_left_width/2, 
                              original_left_y - current_left_height/2 + y_offset);
                lv_obj_set_pos(right_eye_, 
                              original_right_x - current_right_width/2, 
                              original_right_y - current_right_height/2 + y_offset);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head down
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadDown(10); // tilt the head down to show embarrassment
            }
            
            // add a slight left-right shake effect
            const int shake_steps = 6;
            const int shake_delay_ms = 80;
            const int shake_offset = 2;
            
            for (int i = 0; i < shake_steps; i++) {
                // small left-right shake
                int offset_x = (i % 2 == 0) ? shake_offset : -shake_offset;
                
                // update the position
                lv_obj_set_pos(left_eye_, 
                              lv_obj_get_x(left_eye_) + offset_x, 
                              lv_obj_get_y(left_eye_));
                lv_obj_set_pos(right_eye_, 
                              lv_obj_get_x(right_eye_) + offset_x, 
                              lv_obj_get_y(right_eye_));
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(shake_delay_ms));
            }
            
            // hold the embarrassed state for a while
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(left_eye_, original_left_x - original_left_width_/2, original_left_y - original_left_height_/2);
            lv_obj_set_pos(right_eye_, original_right_x - original_right_width_/2, original_right_y - original_right_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteEmbarrassedAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteFunnyAnimation() {
    ESP_LOGI(TAG, "Executing funny expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteFunnyAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size
            // unused variable, commented out
            // const int original_left_width_ = left_eye_width_;
            // unused variable, commented out
            // const int original_left_height_ = left_eye_height_;
            // unused variable, commented out
            // const int original_right_width_ = right_eye_width_;
            // unused variable, commented out
            // const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // goofy expression - one eye big and one eye small
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // create the animation effect
            const int animation_steps = 10;
            const int animation_delay_ms = 15;
            
            // the left eye grows and the right eye shrinks
            for (int step = 0; step < animation_steps; step++) {
                float progress = (float)step / animation_steps;
                
                // enlarge the left eye to 1.5x
                float left_scale = 1.0f + progress * 0.5f;
                int left_width = original_left_width_ * left_scale;
                int left_height = original_left_height_ * left_scale;
                
                // shrink the right eye to 0.6x
                float right_scale = 1.0f - progress * 0.4f;
                int right_width = original_right_width_ * right_scale;
                int right_height = original_right_height_ * right_scale;
                
                // update the eye size and position
                lv_obj_set_size(left_eye_, left_width, left_height);
                lv_obj_set_pos(left_eye_, left_eye_x_ - left_width/2, left_eye_y_ - left_height/2);
                
                lv_obj_set_size(right_eye_, right_width, right_height);
                lv_obj_set_pos(right_eye_, right_eye_x_ - right_width/2, right_eye_y_ - right_height/2);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // if there is a servo controller, tilt the head sideways
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadMove(10, 5, 10); // tilt right, slightly up
            }
            
            // hold the goofy state for a while
            vTaskDelay(pdMS_TO_TICKS(800));
            
            // restore the original state
            lv_obj_set_size(left_eye_, original_left_width_, original_left_height_);
            lv_obj_set_size(right_eye_, original_right_width_, original_right_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_left_width_/2, left_eye_y_ - original_left_height_/2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_right_width_/2, right_eye_y_ - original_right_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteFunnyAnimation: Failed to acquire display lock");
        }
    }
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
}

void EmojiController::ExecuteAngerAnimation() {
    ESP_LOGI(TAG, "Executing angry expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteAngerAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
                        // save the original size
            const int original_left_width_ = left_eye_width_;
            const int original_left_height_ = left_eye_height_;
            const int original_right_width_ = right_eye_width_;
            const int original_right_height_ = right_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
// create the angry expression - based on the sad expression but with the left and right triangles swapped
            // first make sure the eyes are visible
            lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
            
            // get the eye position and size
            int offset = left_eye_height_ / 4;  // reduce the initial offset
            
            // create the left and right eye triangle objects
            lv_obj_t* left_eye_mask = nullptr;
            lv_obj_t* right_eye_mask = nullptr;
            
            // reference the sad expression implementation but swap the left and right triangles
            // draw multiple triangles in a loop, gradually shifting position to create the animation
            for (int i = 0; i < 10; i++) {
                // if the triangle objects were already created, delete them first
                if (left_eye_mask) lv_obj_del(left_eye_mask);
                if (right_eye_mask) lv_obj_del(right_eye_mask);
                
                // create the left eye triangle - using the right eye's sad-expression triangle shape
                left_eye_mask = lv_obj_create(emoji_screen_);
                lv_obj_set_style_bg_color(left_eye_mask, lv_color_white(), 0); // set to white
                lv_obj_set_style_border_width(left_eye_mask, 0, 0);
                lv_obj_set_style_radius(left_eye_mask, 0, 0); // set to a right angle
                
                // compute the happy expression triangle coordinates - using the right eye's parameters
                int happy_rx1 = right_eye_x_ + right_eye_width_/2 + 5;
                int happy_ry1 = right_eye_y_ + offset - 3;
                int happy_rx2 = right_eye_x_ - right_eye_width_/2 - 5;
                int happy_ry2 = right_eye_y_ + 5 + offset;
                int happy_rx3 = right_eye_x_ + right_eye_width_/2 + 5;
                int happy_ry3 = right_eye_y_ + right_eye_height_ + offset + 3;
                
                // compute the sad expression triangle coordinates (vertically flipped) - using the right eye's parameters applied to the left eye
                int sad_rx1 = happy_rx1;
                int sad_ry1 = 2 * right_eye_y_ - happy_ry1;
                int sad_rx2 = happy_rx2;
                // int sad_ry2 = 2 \* right_eye_y_ - happy_ry2;
                // int sad_rx3 = happy_rx3;
                int sad_ry3 = 2 * right_eye_y_ - happy_ry3;
                
                // adjust the coordinates to fit the left eye position
                int left_offset_x = left_eye_x_ - right_eye_x_;
                
                // set the triangle size and position
                int rtri_width = sad_rx1 - sad_rx2 + 15;
                int rtri_height = abs(sad_ry3 - sad_ry1) + 15;
                
                // determine the top-left coordinate (take the smallest x and y)
                int rtop_left_x = sad_rx2 - 8 + left_offset_x;
                int rtop_left_y = std::min(sad_ry1, sad_ry3) - 15;  
                
                lv_obj_set_size(left_eye_mask, rtri_width, rtri_height);
                lv_obj_set_pos(left_eye_mask, rtop_left_x, rtop_left_y);
                
                // set the rotation and tilt of the left eye triangle - using the right eye's sad-expression parameters
                lv_obj_set_style_transform_angle(left_eye_mask, 150, 0); // 15 degrees, in units of 0.1 degree
                // set the rotation center to the triangle's bottom-right corner
                lv_obj_set_style_transform_pivot_x(left_eye_mask, rtri_width, 0);
                lv_obj_set_style_transform_pivot_y(left_eye_mask, rtri_height, 0);
                
                // create the right eye triangle - using the left eye's sad-expression triangle shape
                right_eye_mask = lv_obj_create(emoji_screen_);
                lv_obj_set_style_bg_color(right_eye_mask, lv_color_white(), 0); // set to white
                lv_obj_set_style_border_width(right_eye_mask, 0, 0);
                lv_obj_set_style_radius(right_eye_mask, 0, 0); // set to a right angle
                
                // compute the happy expression triangle coordinates - using the left eye's parameters
                int happy_x1 = left_eye_x_ - left_eye_width_/2 - 5;
                int happy_y1 = left_eye_y_ + offset - 3;
                int happy_x2 = left_eye_x_ + left_eye_width_/2 + 5;
                int happy_y2 = left_eye_y_ + 5 + offset;
                int happy_x3 = left_eye_x_ - left_eye_width_/2 - 5;
                int happy_y3 = left_eye_y_ + left_eye_height_ + offset + 3;
                
                // compute the sad expression triangle coordinates (vertically flipped) - using the left eye's parameters applied to the right eye
                int sad_x1 = happy_x1;
                int sad_y1 = 2 * left_eye_y_ - happy_y1;
                int sad_x2 = happy_x2;
                // int sad_y2 = 2 \* left_eye_y_ - happy_y2;
                // int sad_x3 = happy_x3;
                int sad_y3 = 2 * left_eye_y_ - happy_y3;
                
                // adjust the coordinates to fit the right eye position
                int right_offset_x = right_eye_x_ - left_eye_x_;
                
                // set the triangle size and position
                int tri_width = sad_x2 - sad_x1 + 15;
                int tri_height = abs(sad_y3 - sad_y1) + 15;
                
                // determine the top-left coordinate (take the smallest x and y)
                int top_left_x = sad_x1 - 8 + right_offset_x;
                int top_left_y = std::min(sad_y1, sad_y3) - 15;  
                
                lv_obj_set_size(right_eye_mask, tri_width, tri_height);
                lv_obj_set_pos(right_eye_mask, top_left_x, top_left_y);
                
                // set the rotation and tilt of the right eye triangle - using the left eye's sad-expression parameters
                lv_obj_set_style_transform_angle(right_eye_mask, -150, 0); // -15 degrees, in units of 0.1 degree
                // set the rotation center to the triangle's bottom-left corner
                lv_obj_set_style_transform_pivot_x(right_eye_mask, 0, 0);
                lv_obj_set_style_transform_pivot_y(right_eye_mask, tri_height, 0);
                
                // update the offset to match the reference code
                offset -= 2;
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay to create the animation effect
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            
            // if there is a servo controller, shake the head to show anger
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadShake(1);
            }
            
            // display for a while
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            // delete the created objects
            if (left_eye_mask) lv_obj_del(left_eye_mask);
            if (right_eye_mask) lv_obj_del(right_eye_mask);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteAngerAnimation: Failed to acquire display lock");
        }
    }
    
    // add a short delay to make sure the display update completes
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
    
    // restore the original state
    EyeCenter();
}

void EmojiController::ExecuteSurpriseAnimation() {
    ESP_LOGI(TAG, "Executing surprised expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteSurpriseAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // implement the effect where the eyes first shrink 10%, return to the default size, then enlarge 10%
            // save the original size
            const int original_width = left_eye_width_;
            const int original_height = left_eye_height_;
            const int original_corner_radius_ = ref_corner_radius_;
            
            // animation parameters
            const float scale_factors[] = {0.9f, 1.0f, 1.1f, 1.0f};  // shrink 10%, restore, enlarge 10%, restore
            const int animation_steps = 5;  // steps per phase
            const int animation_delay_ms = 15;  // delay per step
            
            // execute the animation
            for (int phase = 0; phase < 4; phase++) {  // 4 phases: shrink, restore, enlarge, restore
                float start_factor = (phase == 0) ? 1.0f : scale_factors[phase-1];
                float end_factor = scale_factors[phase];
                
                // smooth transition within the current phase
                for (int step = 0; step < animation_steps; step++) {
                    // compute the scale factor for the current step
                    float progress = (float)step / animation_steps;
                    float current_factor = start_factor + (end_factor - start_factor) * progress;
                    
                    // compute the current size
                    int current_width = original_width * current_factor;
                    int current_height = original_height * current_factor;
                    int current_corner_radius = std::max(1, (int)(original_corner_radius_ * current_factor));
                    
                    // update the left eye size and position
                    lv_obj_set_size(left_eye_, current_width, current_height);
                    lv_obj_set_pos(left_eye_, left_eye_x_ - current_width/2, left_eye_y_ - current_height/2);
                    
                    // update the right eye size and position
                    lv_obj_set_size(right_eye_, current_width, current_height);
                    lv_obj_set_pos(right_eye_, right_eye_x_ - current_width/2, right_eye_y_ - current_height/2);
                    
                    // update the corner radius
                    lv_obj_set_style_radius(left_eye_, current_corner_radius, 0);
                    lv_obj_set_style_radius(right_eye_, current_corner_radius, 0);
                    
                    // refresh the display
                    lv_refr_now(NULL);
                    
                    // short delay
                    vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
                }
                
                // pause briefly at the end of a phase, especially for the enlarge and restore phases
                if (phase == 2) {  // after the enlarge phase ends
                    vTaskDelay(pdMS_TO_TICKS(200));  // hold the enlarged state for a while
                }
            }
            
            // if there is a servo controller, tilt the head up to show surprise
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadUp(SERVO_OFFSET_Y/2);
            }
            
            // display for a while
            vTaskDelay(pdMS_TO_TICKS(500));
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteSurpriseAnimation: Failed to acquire display lock");
        }
    }
    
    // add a short delay to make sure the display update completes
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
    
    // restore the original state
    EyeCenter();
}

void EmojiController::ExecuteWakeupAnimation() {
    ESP_LOGI(TAG, "Executing wake-up expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteWakeupAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter();
    
    // use DisplayLockGuard to ensure thread safety
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
            lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteWakeupAnimation: Failed to acquire display lock");
        }
    }
    
    // display for a while
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // restore the original state
    EyeCenter();
}

void EmojiController::ExecuteSleepAnimation() {
    ESP_LOGI(TAG, "Executing sleep expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteSleepAnimation: screen or eye object does not exist");
        return;
    }
    
    // use DisplayLockGuard to ensure thread safety
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // implemented exactly like eye_sleep in boardemoji.ino
            // set the eye height to 2 pixels only, keeping the width unchanged
            left_eye_height_ = 2;
            right_eye_height_ = 2;
            
            // update the eye display
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
            lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
            
            // refresh the display
            lv_refr_now(NULL);
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteSleepAnimation: Failed to acquire display lock");
        }
    }
    
    // if there is a servo controller, tilt the head down
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadDown(SERVO_OFFSET_Y);
    }
    
    // display for a while
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void EmojiController::EyeConfused() {
    PlayAnimation(AnimationType::CONFUSED);
}

void EmojiController::ExecuteConfusedAnimation() {
    ESP_LOGI(TAG, "Executing confused expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteConfusedAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter();
    
    // use DisplayLockGuard to ensure thread safety
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_ / 2);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/4);
            lv_obj_set_size(right_eye_, right_eye_width_, left_eye_height_ / 2);
            lv_obj_set_pos(right_eye_, right_eye_x_ - left_eye_width_/2, right_eye_y_ - left_eye_height_/4);
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteConfusedAnimation: Failed to acquire display lock");
        }
    }
    
    // display for a while
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // restore the original state
    EyeCenter();
}

void EmojiController::ExecuteLookLeftAnimation() {
    ESP_LOGI(TAG, "Executing look-left animation");
    
    // perform the expression and servo movement at the same time
    // if there is a servo controller, turn it to the left
    if (servo_controller_ != nullptr) {
        // start the servo movement and the expression animation at the same time
        servo_controller_->HeadMove(-SERVO_OFFSET_X, 0, SERVO_DELAY);
        ESP_LOGI(TAG, "Servo turning left");
    }
    
    // check whether the object exists; if not, only perform the servo movement
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteLookLeftAnimation: screen or eye object does not exist, executing servo movement only");
        return;
    }
    
    // look-left animation
    MoveEye(-1);
    
    // hold for a while
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // recovery phase
    // define the same parameters as the MoveEye function
    int direction = -1;
    int direction_movement_amplitude = 2;
    
    // phase 3: start recovering, moving in the opposite direction while the eye shrinks
    for (int i = 0; i < 3; i++) {
        if (display_) {
            DisplayLockGuard lock(display_);
            
            left_eye_x_ -= direction_movement_amplitude * direction;
            right_eye_x_ -= direction_movement_amplitude * direction;
            right_eye_height_ -= 5;
            left_eye_height_ -= 5;
            
            // during look-left recovery, the left eye shrinks
            left_eye_height_ -= 1;  // adjusted to match the deformation amplitude of the original code
            left_eye_width_ -= 1;
            
            // update the display directly inside the lock without using DrawEmoji
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
            lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // phase 4: keep recovering while the eye regains its height
    for (int i = 0; i < 3; i++) {
        if (display_) {
            DisplayLockGuard lock(display_);
            
            left_eye_x_ -= direction_movement_amplitude * direction;
            right_eye_x_ -= direction_movement_amplitude * direction;
            right_eye_height_ += 5;
            left_eye_height_ += 5;
            
            // during look-left recovery, the left eye keeps shrinking
            left_eye_height_ -= 1;  // adjusted to match the deformation amplitude of the original code
            left_eye_width_ -= 1;
            
            // update the display directly inside the lock without using DrawEmoji
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
            lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // if there is a servo controller, restore it to the center position
    if (servo_controller_ != nullptr) {
        // slowly return to the center position using the same delay as the movement
        servo_controller_->HeadCenter(SERVO_DELAY);
    }
    
    // fully return to the center position
    EyeCenter();
}

void EmojiController::ExecuteLookRightAnimation() {
    ESP_LOGI(TAG, "Executing look-right animation");
    
    // perform the expression and servo movement at the same time
    // if there is a servo controller, turn it to the right
    if (servo_controller_ != nullptr) {
        // start the servo movement and the expression animation at the same time
        servo_controller_->HeadMove(SERVO_OFFSET_X, 0, SERVO_DELAY);
        ESP_LOGI(TAG, "Servo turning right");
    }
    
    // check whether the object exists; if not, only perform the servo movement
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteLookRightAnimation: screen or eye object does not exist, executing servo movement only");
        return;
    }
    
    // look-right animation
    MoveEye(1);
    
    // hold for a while
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // recovery phase
    // define the same parameters as the MoveEye function
    int direction = 1;
    int direction_movement_amplitude = 2;
    
    // phase 3: start recovering, moving in the opposite direction while the eye shrinks
    for (int i = 0; i < 3; i++) {
        if (display_) {
            DisplayLockGuard lock(display_);
            
            left_eye_x_ -= direction_movement_amplitude * direction;
            right_eye_x_ -= direction_movement_amplitude * direction;
            right_eye_height_ -= 5;
            left_eye_height_ -= 5;
            
            // during look-right recovery, the right eye shrinks
            right_eye_height_ -= 1;  // adjusted to match the deformation amplitude of the original code
            right_eye_width_ -= 1;
            
            // update the display directly inside the lock without using DrawEmoji
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
            lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // phase 4: keep recovering while the eye regains its height
    for (int i = 0; i < 3; i++) {
        if (display_) {
            DisplayLockGuard lock(display_);
            
            left_eye_x_ -= direction_movement_amplitude * direction;
            right_eye_x_ -= direction_movement_amplitude * direction;
            right_eye_height_ += 5;
            left_eye_height_ += 5;
            
            // during look-right recovery, the right eye keeps shrinking
            right_eye_height_ -= 1;  // adjusted to match the deformation amplitude of the original code
            right_eye_width_ -= 1;
            
            // update the display directly inside the lock without using DrawEmoji
            lv_obj_set_size(left_eye_, left_eye_width_, left_eye_height_);
            lv_obj_set_pos(left_eye_, left_eye_x_ - left_eye_width_/2, left_eye_y_ - left_eye_height_/2);
            lv_obj_set_size(right_eye_, right_eye_width_, right_eye_height_);
            lv_obj_set_pos(right_eye_, right_eye_x_ - right_eye_width_/2, right_eye_y_ - right_eye_height_/2);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // if there is a servo controller, restore it to the center position
    if (servo_controller_ != nullptr) {
        // slowly return to the center position using the same delay as the movement
        servo_controller_->HeadCenter(SERVO_DELAY);
    }
    
    // fully return to the center position
    EyeCenter();
}

void EmojiController::ExecuteHeadNodAnimation() {
    ESP_LOGI(TAG, "Executing head-nod animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteHeadNodAnimation: screen or eye object does not exist");
        return;
    }
    
    // nodding animation
    // if there is a servo controller, nod the head
    if (servo_controller_ != nullptr) {
        // use the HeadNod method in ServoController instead of calling HeadMove directly
        servo_controller_->HeadNod(10);
        
        // restore the center position
        servo_controller_->HeadCenter(10);
    } else {
        // if there is no servo controller, simulate the nod with the eyes
        for (int i = 0; i < 3; i++) {
            // eyes down
            left_eye_y_ += 10;
            right_eye_y_ += 10;
            DrawEmoji(false);
            vTaskDelay(pdMS_TO_TICKS(200));
            
            // eyes restore
            left_eye_y_ -= 10;
            right_eye_y_ -= 10;
            DrawEmoji(false);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
    
    // display for a while
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // restore the original state
    EyeCenter();
}

void EmojiController::ExecuteHeadShakeAnimation() {
    ESP_LOGI(TAG, "Executing head-shake animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteHeadShakeAnimation: screen or eye object does not exist");
        return;
    }
    
    // head-shake animation
    // if there is a servo controller, shake the head
    if (servo_controller_ != nullptr) {
        // implement the head shake based on the original Arduino code
        servo_controller_->HeadShake(1);
        
        // restore the center position
        servo_controller_->HeadCenter(1);
    } else {
        // if there is no servo controller, simulate the shake with the eyes
        // move the eyes left and right
        left_eye_x_ -= 10;
        right_eye_x_ -= 10;
        DrawEmoji(false);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        left_eye_x_ += 20;
        right_eye_x_ += 20;
        DrawEmoji(false);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        left_eye_x_ -= 20;
        right_eye_x_ -= 20;
        DrawEmoji(false);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        left_eye_x_ += 20;
        right_eye_x_ += 20;
        DrawEmoji(false);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        left_eye_x_ -= 20;
        right_eye_x_ -= 20;
        DrawEmoji(false);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        left_eye_x_ += 10;
        right_eye_x_ += 10;
        DrawEmoji(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // display for a while
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // restore the original state
    EyeCenter();
}

void EmojiController::ExecuteHeadRollAnimation() {
    ESP_LOGI(TAG, "Executing head-roll animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteHeadRollAnimation: screen or eye object does not exist");
        return;
    }
    
    // spinning animation
    // if there is a servo controller, spin it
    if (servo_controller_ != nullptr) {
        // implement the spin based on the original Arduino code
        servo_controller_->HeadCenter();
        servo_controller_->HeadDown(SERVO_OFFSET_Y/2+5);
        servo_controller_->HeadMove(SERVO_OFFSET_X, -SERVO_OFFSET_Y/2, SERVO_DELAY);
        servo_controller_->HeadMove(-SERVO_OFFSET_X, -SERVO_OFFSET_Y/2, SERVO_DELAY);
        servo_controller_->HeadMove(-SERVO_OFFSET_X, SERVO_OFFSET_Y/2, SERVO_DELAY);
        servo_controller_->HeadMove(SERVO_OFFSET_X, SERVO_OFFSET_Y/2, SERVO_DELAY);
        servo_controller_->HeadMove(-SERVO_OFFSET_X, -SERVO_OFFSET_Y/2, SERVO_DELAY);
        servo_controller_->HeadMove(SERVO_OFFSET_X, -SERVO_OFFSET_Y/2, SERVO_DELAY);
        servo_controller_->HeadMove(SERVO_OFFSET_X, SERVO_OFFSET_Y/2, SERVO_DELAY);
        servo_controller_->HeadMove(-SERVO_OFFSET_X, SERVO_OFFSET_Y/2, SERVO_DELAY);
        servo_controller_->HeadCenter();
    } else {
        // if there is no servo controller, simulate the spin with the eyes
        // move the eyes in a circle
        int radius = 5;
        for (int angle = 0; angle < 360; angle += 30) {
            float rad = angle * 3.14159 / 180.0;
            int x_offset = radius * cos(rad);
            int y_offset = radius * sin(rad);
            
            left_eye_x_ = DISPLAY_WIDTH / 2 - ref_eye_width_ / 2 - ref_space_between_eye_ / 2 + x_offset;
            left_eye_y_ = DISPLAY_HEIGHT / 2 + y_offset;
            right_eye_x_ = DISPLAY_WIDTH / 2 + ref_eye_width_ / 2 + ref_space_between_eye_ / 2 + x_offset;
            right_eye_y_ = DISPLAY_HEIGHT / 2 + y_offset;
            
            DrawEmoji(false);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    // display for a while
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // restore the original state
    EyeCenter();
}

void EmojiController::InitEmoji() {
    ESP_LOGI(TAG, "Initializing emoji");
    
    // create the animation queue
    if (animation_queue_ == nullptr) {
        animation_queue_ = xQueueCreate(ANIMATION_QUEUE_SIZE, sizeof(AnimationMessage));
    }
    
    // create the animation task
    if (animation_task_handle_ == nullptr) {
        BaseType_t task_created = xTaskCreate(
            AnimationTask,
            "AnimationTask",
            4096,
            this,
            5,
            &animation_task_handle_
        );
        
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create animation task");
        }
    }
    
    // create the animation timer task
    if (animation_timer_task_handle_ == nullptr) {
        BaseType_t task_created = xTaskCreate(
            AnimationTimerTask,
            "AnimationTimerTask",
            4096,
            this,
            5,
            &animation_timer_task_handle_);
        
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create animation timer task");
        }
    }
    
    ESP_LOGI(TAG, "Emoji initialization complete");
}


void EmojiController::EyeUp() {
    ESP_LOGI(TAG, "Eyes up");
    
    // if there is a servo controller, move the head at the same time
    if (servo_controller_) {
        servo_controller_->HeadUp();
    }
    
    // use the existing animation logic
    AnimationMessage msg;
    msg.type = AnimationType::HEAD_NOD;  // use the nodding animation for looking up
    msg.param = 0;
    
    // send the animation message to the queue
    if (animation_queue_) {
        xQueueSend(animation_queue_, &msg, 0);
    }
}

void EmojiController::EyeDown() {
    ESP_LOGI(TAG, "Eyes down");
    
    // if there is a servo controller, move the head
    if (servo_controller_) {
        servo_controller_->HeadDown();
    }
    
    // do not show the sad expression, only move the head
    // comment out the original animation logic
    /*
    AnimationMessage msg;
    msg.type = AnimationType::SAD;  // use the sad animation for looking down
    msg.param = 0;
    
    // send the animation message to the queue
    if (animation_queue_) {
        xQueueSend(animation_queue_, &msg, 0);
    }
    */
}

void EmojiController::ExecuteAwkwardAnimation() {
    ESP_LOGI(TAG, "Executing embarrassed expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteAwkwardAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size
            const int original_width = left_eye_width_;
            const int original_height = left_eye_height_;
            
            // create three vertical lines (sweat drops)
            lv_obj_t* sweat_line1 = nullptr;
            lv_obj_t* sweat_line2 = nullptr;
            lv_obj_t* sweat_line3 = nullptr;
            
            // animation parameters
            const int animation_steps = 10;
            const int animation_delay_ms = 20;
            const int target_eye_height = 4;  // the eye height shrinks to 4 pixels
            
            // animation where the eye height gradually shrinks
            for (int step = 0; step < animation_steps; step++) {
                // compute the current height
                float progress = (float)step / animation_steps;
                int current_height = original_height - progress * (original_height - target_eye_height);
                
                // update the left eye size and position
                lv_obj_set_size(left_eye_, original_width, current_height);
                lv_obj_set_pos(left_eye_, left_eye_x_ - original_width/2, left_eye_y_ - current_height/2);
                
                // update the right eye size and position
                lv_obj_set_size(right_eye_, original_width, current_height);
                lv_obj_set_pos(right_eye_, right_eye_x_ - original_width/2, right_eye_y_ - current_height/2);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // create three vertical lines (sweat drops) at the top right
            const int sweat_width = 2;
            const int sweat_heights[] = {10, 10, 12};
            const int sweat_spacing = 4;  // spacing between the vertical lines
            
            // adjust the line positions to keep them within the visible screen area
            // the original positions may be outside the screen bounds
            const int sweat_x_start = right_eye_x_ + 15;  // start to the right of the right eye, but not too far
            const int sweat_y = 10;  // near the top of the screen to make sure it is visible
            
            ESP_LOGI(TAG, "Created vertical line 1, position: (%d, %d), size: %dx%d",
                     sweat_x_start, sweat_y, sweat_width, sweat_heights[0]);
            
            // create the first vertical line
            sweat_line1 = lv_obj_create(emoji_screen_);
            lv_obj_set_style_bg_color(sweat_line1, lv_color_black(), 0);  // set to black, which renders as white on the OLED
            lv_obj_set_style_border_width(sweat_line1, 0, 0);
            lv_obj_set_style_radius(sweat_line1, 1, 0);  // corner radius
            lv_obj_set_size(sweat_line1, sweat_width, sweat_heights[0]);
            lv_obj_set_pos(sweat_line1, sweat_x_start, sweat_y);
            lv_obj_clear_flag(sweat_line1, LV_OBJ_FLAG_HIDDEN);  // make sure it is not hidden
            
            ESP_LOGI(TAG, "Created vertical line 2, position: (%d, %d), size: %dx%d",
                     sweat_x_start + sweat_width + sweat_spacing, sweat_y, sweat_width, sweat_heights[1]);
            
            // create the second vertical line
            sweat_line2 = lv_obj_create(emoji_screen_);
            lv_obj_set_style_bg_color(sweat_line2, lv_color_black(), 0);  // set to black, which renders as white on the OLED
            lv_obj_set_style_border_width(sweat_line2, 0, 0);
            lv_obj_set_style_radius(sweat_line2, 1, 0);
            lv_obj_set_size(sweat_line2, sweat_width, sweat_heights[1]);
            lv_obj_set_pos(sweat_line2, sweat_x_start + sweat_width + sweat_spacing, sweat_y);
            lv_obj_clear_flag(sweat_line2, LV_OBJ_FLAG_HIDDEN);  // make sure it is not hidden
            
            ESP_LOGI(TAG, "Created vertical line 3, position: (%d, %d), size: %dx%d",
                     sweat_x_start + 2 * (sweat_width + sweat_spacing), sweat_y, sweat_width, sweat_heights[2]);
            
            // create the third vertical line
            sweat_line3 = lv_obj_create(emoji_screen_);
            lv_obj_set_style_bg_color(sweat_line3, lv_color_black(), 0);  // set to black, which renders as white on the OLED
            lv_obj_set_style_border_width(sweat_line3, 0, 0);
            lv_obj_set_style_radius(sweat_line3, 1, 0);
            lv_obj_set_size(sweat_line3, sweat_width, sweat_heights[2]);
            lv_obj_set_pos(sweat_line3, sweat_x_start + 2 * (sweat_width + sweat_spacing), sweat_y);
            lv_obj_clear_flag(sweat_line3, LV_OBJ_FLAG_HIDDEN);  // make sure it is not hidden
            
            // refresh the display
            lv_refr_now(NULL);
            
            // if there is a servo controller, tilt it down-right to show embarrassment
            if (servo_controller_ != nullptr) {
                servo_controller_->HeadMove(10, -5, 1);  // slight right tilt
            }
            
            // display for a while
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            // delete the created objects
            if (sweat_line1) lv_obj_del(sweat_line1);
            if (sweat_line2) lv_obj_del(sweat_line2);
            if (sweat_line3) lv_obj_del(sweat_line3);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteAwkwardAnimation: Failed to acquire display lock");
        }
    }
    
    // add a short delay to make sure the display update completes
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
    
    // restore the original state
    EyeCenter();
}

void EmojiController::ExecuteCryAnimation() {
    ESP_LOGI(TAG, "Executing crying expression animation");
    
    // check whether the object exists
    if (emoji_screen_ == nullptr || left_eye_ == nullptr || right_eye_ == nullptr) {
        ESP_LOGW(TAG, "ExecuteCryAnimation: screen or eye object does not exist");
        return;
    }
    
    // center the eyes first
    EyeCenter(false);
    
    if (display_) {
        try {
            DisplayLockGuard lock(display_);
            
            // save the original size and position
            const int original_width = left_eye_width_;
            const int original_height = left_eye_height_;
            const int original_left_y = left_eye_y_;
            const int original_right_y = right_eye_y_;
            
            // create the teardrop object
            lv_obj_t* left_tear = nullptr;
            lv_obj_t* right_tear = nullptr;
            
            // animation parameters
            const int animation_steps = 10;
            const int animation_delay_ms = 30;
            const int eye_move_up = 8;  // pixels to move the lower part of the eyes up
            const int tear_size = 16;   // teardrop size
            
            // animation moving the lower part of the eyes up
            for (int step = 0; step < animation_steps; step++) {
                // compute the current upward shift
                float progress = (float)step / animation_steps;
                int current_move_up = progress * eye_move_up;
                
                // update the left eye size and position - lower part moved up
                lv_obj_set_size(left_eye_, original_width, original_height - current_move_up);
                lv_obj_set_pos(left_eye_, left_eye_x_ - original_width/2, original_left_y - original_height/2);
                
                // update the right eye size and position - lower part moved up
                lv_obj_set_size(right_eye_, original_width, original_height - current_move_up);
                lv_obj_set_pos(right_eye_, right_eye_x_ - original_width/2, original_right_y - original_height/2);
                
                // refresh the display
                lv_refr_now(NULL);
                
                // short delay
                vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
            }
            
            // create the left teardrop
            left_tear = lv_obj_create(emoji_screen_);
            lv_obj_set_style_bg_color(left_tear, lv_color_black(), 0);  // set to black, which renders as white on the OLED
            lv_obj_set_style_border_width(left_tear, 0, 0);
            lv_obj_set_style_radius(left_tear, tear_size/2, 0);  // round teardrop
            lv_obj_set_size(left_tear, tear_size, tear_size);
            
            // create the right teardrop
            right_tear = lv_obj_create(emoji_screen_);
            lv_obj_set_style_bg_color(right_tear, lv_color_black(), 0);  // set to black, which renders as white on the OLED
            lv_obj_set_style_border_width(right_tear, 0, 0);
            lv_obj_set_style_radius(right_tear, tear_size/2, 0);  // round teardrop
            lv_obj_set_size(right_tear, tear_size, tear_size);
            
            // animation of the teardrop falling from below the eye, repeated 3 times
            const int tear_start_y = original_left_y + original_height/2 - eye_move_up;
            const int tear_end_y = tear_start_y + 20;  // teardrop falling distance
            
            // repeat the teardrop animation 3 times
            for (int repeat = 0; repeat < 3; repeat++) {
                // reset the teardrop position to the start each time
                lv_obj_set_pos(left_tear, left_eye_x_ - tear_size/2, tear_start_y);
                lv_obj_set_pos(right_tear, right_eye_x_ - tear_size/2, tear_start_y);
                
                // teardrop falling animation
                for (int step = 0; step < animation_steps; step++) {
                    // compute the current teardrop position
                    float progress = (float)step / animation_steps;
                    int current_tear_y = tear_start_y + progress * (tear_end_y - tear_start_y);
                    
                    // update the left teardrop position
                    lv_obj_set_pos(left_tear, left_eye_x_ - tear_size/2, current_tear_y);
                    
                    // update the right teardrop position
                    lv_obj_set_pos(right_tear, right_eye_x_ - tear_size/2, current_tear_y);
                    
                    // refresh the display
                    lv_refr_now(NULL);
                    
                    // short delay
                    vTaskDelay(pdMS_TO_TICKS(animation_delay_ms));
                }
                
                // brief pause after each teardrop fall
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            
            // display for a while
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // delete the teardrop objects
            if (left_tear) lv_obj_del(left_tear);
            if (right_tear) lv_obj_del(right_tear);
            
            // restore the eyes to their original size and position
            lv_obj_set_size(left_eye_, original_width, original_height);
            lv_obj_set_pos(left_eye_, left_eye_x_ - original_width/2, original_left_y - original_height/2);
            lv_obj_set_size(right_eye_, original_width, original_height);
            lv_obj_set_pos(right_eye_, right_eye_x_ - original_width/2, original_right_y - original_height/2);
            
            // refresh the display
            lv_refr_now(NULL);
            
        } catch (...) {
            ESP_LOGW(TAG, "ExecuteCryAnimation: Failed to acquire display lock");
        }
    }
    
    // add a short delay to make sure the display update completes
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // restore the servo position
    if (servo_controller_ != nullptr) {
        servo_controller_->HeadCenter(10);
    }
    
    // restore the original state
    EyeCenter();
}

// suspend the LVGL task (empty implementation to prevent link errors)
void EmojiController::SuspendLVGLTask() {
    // TODO: implement the logic to suspend the LVGL task
}
