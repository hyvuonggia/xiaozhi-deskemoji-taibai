/**
 * @file esp32_uno_d1_r32_board.cc
 * @brief ESP32 UNO D1 R32 development board implementation
 * 
 * This file implements the features of the ESP32 UNO D1 R32 development board, including audio I/O, display, and servo control
 */

#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "display/oled_display.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <cstring>
#include <cstdlib>
#include <ctime>

#define TAG "ESP32-UNO-D1-R32"

// Declare the font
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

// Servo configuration
#define LEDC_TIMER           LEDC_TIMER_0
#define LEDC_MODE            LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES        LEDC_TIMER_13_BIT  // Resolution
#define LEDC_FREQUENCY       50                 // 50 Hz PWM frequency

// Servo angle range
#define SERVO_MIN_ANGLE      0
#define SERVO_MAX_ANGLE      180
#define SERVO_MID_ANGLE      90

// Servo pulse width range (unit: microseconds)
#define SERVO_MIN_PULSEWIDTH 500   // 0.5ms
#define SERVO_MAX_PULSEWIDTH 2500  // 2.5ms

// Servo type
typedef enum {
    SERVO_HORIZONTAL = 0,  // Horizontal servo
    SERVO_VERTICAL = 1,    // Vertical servo
    SERVO_MAX
} servo_type_t;

// Servo channel configuration
static ledc_channel_t servo_channels[SERVO_MAX] = {
    LEDC_CHANNEL_0,  // Horizontal servo
    LEDC_CHANNEL_1   // Vertical servo
};

// Servo pin configuration
static uint8_t servo_pins[SERVO_MAX] = {
    SERVO_HORIZONTAL_PIN,  // Horizontal servo
    SERVO_VERTICAL_PIN     // Vertical servo
};

// Current servo angle
static uint8_t servo_angles[SERVO_MAX] = {
    SERVO_MID_ANGLE,  // Initial horizontal servo angle
    SERVO_MID_ANGLE   // Initial vertical servo angle
};

// Emotion definitions
typedef enum {
    EMOTION_HAPPY = 0,
    EMOTION_SAD,
    EMOTION_SURPRISED,
    EMOTION_ANGRY,
    EMOTION_NEUTRAL,
    EMOTION_MAX
} emotion_type_t;

// Emotion bitmap data
static const uint8_t emotion_bitmaps[][128] = {
    // Happy emotion
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    },
    // Sad emotion
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    },
    // Other emotion data...
};

// Action sequence definitions
typedef struct {
    uint8_t horizontal_angle;
    uint8_t vertical_angle;
    uint32_t delay_ms;
} servo_action_t;

// Nod action sequence
static const servo_action_t nod_sequence[] = {
    {90, 70, 300},
    {90, 110, 300},
    {90, 70, 300},
    {90, 90, 300}
};

// Shake-head action sequence
static const servo_action_t shake_sequence[] = {
    {70, 90, 300},
    {110, 90, 300},
    {70, 90, 300},
    {90, 90, 300}
};

// Convert angle to duty cycle
static uint32_t angle_to_duty(uint8_t angle)
{
    // Map angle to pulse width
    uint32_t pulse_width = SERVO_MIN_PULSEWIDTH + (angle * (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) / SERVO_MAX_ANGLE);
    
    // Convert pulse width to duty cycle value
    uint32_t duty = (pulse_width * ((1 << LEDC_DUTY_RES) - 1)) / (1000000 / LEDC_FREQUENCY);
    
    return duty;
}

class ESP32UnoD1R32Board : public WifiBoard {
private:
    Button boot_button_;
    
    // Touch sensor related variables
    bool is_touch_active_ = false;
    int64_t touch_start_time_ = 0;
    bool is_recording_ = false;
    TaskHandle_t touch_task_handle_ = nullptr;

    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;

    // Initialize the I2C bus
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    // Initialize the SSD1306 OLED display
    void InitializeSsd1306Display() {
        // SSD1306 configuration
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Installing SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Turn on the display
        ESP_LOGI(TAG, "Turning on display");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }

    // Initialize the servos
    void InitializeServos() {
        ESP_LOGI(TAG, "Initializing servos...");

        // Configure the LEDC timer
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_MODE,
            .duty_resolution = LEDC_DUTY_RES,
            .timer_num = LEDC_TIMER,
            .freq_hz = LEDC_FREQUENCY,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        // Configure the LEDC channel
        for (int i = 0; i < SERVO_MAX; i++) {
            ledc_channel_config_t ledc_channel = {
                .gpio_num = servo_pins[i],
                .speed_mode = LEDC_MODE,
                .channel = servo_channels[i],
                .intr_type = LEDC_INTR_DISABLE,
                .timer_sel = LEDC_TIMER,
                .duty = angle_to_duty(servo_angles[i]),
                .hpoint = 0
            };
            ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
        }

        // Initialize the random number generator
        srand(time(NULL));

        // Move the servos to the center position
        for (int i = 0; i < SERVO_MAX; i++) {
            SetServoAngle((servo_type_t)i, SERVO_MID_ANGLE);
        }

        ESP_LOGI(TAG, "Servo initialization successful");
    }

    // Set the servo angle
    void SetServoAngle(servo_type_t type, uint8_t angle) {
        if (type >= SERVO_MAX) {
            return;
        }

        // Clamp the angle range
        if (angle > SERVO_MAX_ANGLE) {
            angle = SERVO_MAX_ANGLE;
        }

        // Set the duty cycle
        uint32_t duty = angle_to_duty(angle);
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, servo_channels[type], duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, servo_channels[type]));

        // Update the current angle
        servo_angles[type] = angle;
    }

    // Perform the nod action
    void PerformNodAction() {
        ESP_LOGI(TAG, "Performing nodding motion");
        
        for (int i = 0; i < sizeof(nod_sequence) / sizeof(nod_sequence[0]); i++) {
            SetServoAngle(SERVO_HORIZONTAL, nod_sequence[i].horizontal_angle);
            SetServoAngle(SERVO_VERTICAL, nod_sequence[i].vertical_angle);
            vTaskDelay(pdMS_TO_TICKS(nod_sequence[i].delay_ms));
        }
    }

    // Perform the shake-head action
    void PerformShakeAction() {
        ESP_LOGI(TAG, "Performing head-shaking motion");
        
        for (int i = 0; i < sizeof(shake_sequence) / sizeof(shake_sequence[0]); i++) {
            SetServoAngle(SERVO_HORIZONTAL, shake_sequence[i].horizontal_angle);
            SetServoAngle(SERVO_VERTICAL, shake_sequence[i].vertical_angle);
            vTaskDelay(pdMS_TO_TICKS(shake_sequence[i].delay_ms));
        }
    }

    // Display the emotion
    void DisplayEmotion(emotion_type_t emotion) {
        if (emotion >= EMOTION_MAX || !display_) {
            return;
        }

        // Get the text corresponding to the current emotion
        const char* emotion_text = "";
        switch (emotion) {
            case EMOTION_HAPPY:
                emotion_text = "^_^";
                break;
            case EMOTION_SAD:
                emotion_text = "T_T";
                break;
            case EMOTION_SURPRISED:
                emotion_text = "O_O";
                break;
            case EMOTION_ANGRY:
                emotion_text = ">_<";
                break;
            case EMOTION_NEUTRAL:
                emotion_text = "-_-";
                break;
            default:
                emotion_text = "?_?";
                break;
        }
        
        // Use the Display class public method to show the emotion
        display_->SetEmotion(emotion_text);
    }

    // Button configuration
    #define BOOT_BUTTON_GPIO        GPIO_NUM_0   // Onboard BOOT button (system function, do not modify)
    #define TOUCH_SENSOR_GPIO       GPIO_NUM_4   // Touch sensor pin
    #define TOUCH_THRESHOLD         1000         // Touch threshold
    #define BUILTIN_LED_GPIO        GPIO_NUM_2   // Onboard LED

    // Button long-press time definition (milliseconds)
    #define BUTTON_LONG_PRESS_TIME  1000         // 1 second

    // Initialize the touch sensor
    void InitializeTouchSensor() {
        ESP_LOGI(TAG, "Initializing touch sensor...");
        
#if TOUCH_SENSOR_TYPE == 0
        // Use the ESP32 built-in capacitive touch sensor
        ESP_LOGI(TAG, "Using built-in ESP32 capacitive touch sensor (GPIO%d)", TOUCH_SENSOR_GPIO);
        
        // Initialize the touch sensor
        touch_pad_init();
        
        // Set the touch sensor threshold
        touch_pad_config((touch_pad_t)(TOUCH_SENSOR_GPIO - GPIO_NUM_4), TOUCH_THRESHOLD);
        
        // Create the touch sensor monitoring task
        xTaskCreate([](void* arg) {
            ESP32UnoD1R32Board* board = static_cast<ESP32UnoD1R32Board*>(arg);
            uint16_t touch_value;
            bool last_touch_state = false;
            
            while (true) {
                // Read the touch sensor value
                touch_pad_read((touch_pad_t)(TOUCH_SENSOR_GPIO - GPIO_NUM_4), &touch_value);
                
                // Check whether it is touched (a smaller value means a stronger touch)
                bool current_touch_state = (touch_value < TOUCH_THRESHOLD);
                
                // Detect touch state changes
                if (current_touch_state != last_touch_state) {
                    if (current_touch_state) {
                        // Touch started
                        board->is_touch_active_ = true;
                        board->touch_start_time_ = esp_timer_get_time() / 1000;
                        gpio_set_level(BUILTIN_LED_GPIO, 1); // Light the LED to indicate touch
                        
                        // Show a neutral emotion to indicate standby
                        board->DisplayEmotion(EMOTION_NEUTRAL);
                        ESP_LOGI(TAG, "Touch started, preparing...");
                    } else {
                        // Touch ended
                        board->is_touch_active_ = false;
                        int64_t current_time = esp_timer_get_time() / 1000;
                        int64_t touch_duration = current_time - board->touch_start_time_;
                        
                        gpio_set_level(BUILTIN_LED_GPIO, 0); // Turn off the LED
                        
                        if (board->is_recording_) {
                            // If recording, stop recording and send the voice
                            Application::GetInstance().StopListening();
                            board->is_recording_ = false;
                            
                            // Show the thinking emotion
                            board->DisplayEmotion(EMOTION_NEUTRAL);
                            ESP_LOGI(TAG, "Stopping recording and sending audio");
                        } else if (touch_duration < BUTTON_LONG_PRESS_TIME) {
                            // Short touch - toggle the chat state
                            Application::GetInstance().ToggleChatState();
                            
                            // Perform the nod action and show the happy emotion
                            board->PerformNodAction();
                            board->DisplayEmotion(EMOTION_HAPPY);
                            ESP_LOGI(TAG, "Short touch, toggling chat state");
                        }
                    }
                    
                    last_touch_state = current_touch_state;
                }
                
                // Detect a long touch
                if (board->is_touch_active_ && !board->is_recording_) {
                    int64_t current_time = esp_timer_get_time() / 1000;
                    int64_t touch_duration = current_time - board->touch_start_time_;
                    
                    if (touch_duration >= BUTTON_LONG_PRESS_TIME) {
                        // Long touch exceeds the set time, start recording
                        board->is_recording_ = true;
                        Application::GetInstance().StartListening();
                        
                        // Show the recording emotion
                        board->DisplayEmotion(EMOTION_SURPRISED);
                        ESP_LOGI(TAG, "Long touch, starting recording");
                    }
                }
                
                vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50 ms
            }
        }, "touch_monitor", 4096, this, 5, &touch_task_handle_);
#else
        // Use an external 3-wire touch switch module (digital input)
        ESP_LOGI(TAG, "Using external three-wire touch switch module (GPIO%d)", TOUCH_SENSOR_GPIO);
        
        // Configure the GPIO as input
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << TOUCH_SENSOR_GPIO,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
        
        // Create the touch sensor monitoring task
        xTaskCreate([](void* arg) {
            ESP32UnoD1R32Board* board = static_cast<ESP32UnoD1R32Board*>(arg);
            bool last_touch_state = false;
            
            while (true) {
                // Read the touch sensor state (high level means touched)
                bool current_touch_state = gpio_get_level(TOUCH_SENSOR_GPIO);
                
                // Detect touch state changes
                if (current_touch_state != last_touch_state) {
                    if (current_touch_state) {
                        // Touch started
                        board->is_touch_active_ = true;
                        board->touch_start_time_ = esp_timer_get_time() / 1000;
                        gpio_set_level(BUILTIN_LED_GPIO, 1); // Light the LED to indicate touch
                        
                        // Show a neutral emotion to indicate standby
                        board->DisplayEmotion(EMOTION_NEUTRAL);
                        ESP_LOGI(TAG, "Touch started, preparing...");
                    } else {
                        // Touch ended
                        board->is_touch_active_ = false;
                        int64_t current_time = esp_timer_get_time() / 1000;
                        int64_t touch_duration = current_time - board->touch_start_time_;
                        
                        gpio_set_level(BUILTIN_LED_GPIO, 0); // Turn off the LED
                        
                        if (board->is_recording_) {
                            // If recording, stop recording and send the voice
                            Application::GetInstance().StopListening();
                            board->is_recording_ = false;
                            
                            // Show the thinking emotion
                            board->DisplayEmotion(EMOTION_NEUTRAL);
                            ESP_LOGI(TAG, "Stopping recording and sending audio");
                        } else if (touch_duration < BUTTON_LONG_PRESS_TIME) {
                            // Short touch - toggle the chat state
                            Application::GetInstance().ToggleChatState();
                            
                            // Perform the nod action and show the happy emotion
                            board->PerformNodAction();
                            board->DisplayEmotion(EMOTION_HAPPY);
                            ESP_LOGI(TAG, "Short touch, toggling chat state");
                        }
                    }
                    
                    last_touch_state = current_touch_state;
                }
                
                // Detect a long touch
                if (board->is_touch_active_ && !board->is_recording_) {
                    int64_t current_time = esp_timer_get_time() / 1000;
                    int64_t touch_duration = current_time - board->touch_start_time_;
                    
                    if (touch_duration >= BUTTON_LONG_PRESS_TIME) {
                        // Long touch exceeds the set time, start recording
                        board->is_recording_ = true;
                        Application::GetInstance().StartListening();
                        
                        // Show the recording emotion
                        board->DisplayEmotion(EMOTION_SURPRISED);
                        ESP_LOGI(TAG, "Long touch, starting recording");
                    }
                }
                
                vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50 ms
            }
        }, "touch_monitor", 4096, this, 5, &touch_task_handle_);
#endif
        
        ESP_LOGI(TAG, "Touch sensor initialization complete");
    }

    // Initialize the button
    void InitializeButtons() {
        // Configure GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,  // Set the GPIO pins to configure
            .mode = GPIO_MODE_OUTPUT,           // Set to output mode
            .pull_up_en = GPIO_PULLUP_DISABLE,  // Disable pull-up
            .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Disable pull-down
            .intr_type = GPIO_INTR_DISABLE      // Disable interrupts
        };
        gpio_config(&io_conf);  // Apply the configuration

        // BOOT button click event - used only for WiFi configuration reset, does not interfere with system functions
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
                ESP_LOGI(TAG, "Resetting Wi-Fi configuration");
            }
        });
    }

    // IoT initialization, add devices visible to the AI
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

    // Initialize the audio system
    void InitializeAudio() {
        ESP_LOGI(TAG, "Initializing audio system...");
        
        // Configure the GPIO for the MAX98357A amplifier
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << AUDIO_I2S_SPK_GPIO_DOUT);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // Enable the pull-up resistor to improve signal stability
        gpio_config(&io_conf);
        
        // Configure the I2S pins
        gpio_config_t i2s_io_conf = {};
        i2s_io_conf.intr_type = GPIO_INTR_DISABLE;
        i2s_io_conf.mode = GPIO_MODE_OUTPUT;
        i2s_io_conf.pin_bit_mask = (1ULL << AUDIO_I2S_SPK_GPIO_BCLK) | (1ULL << AUDIO_I2S_SPK_GPIO_LRCK);
        i2s_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        i2s_io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // Enable the pull-up resistor to improve signal stability
        gpio_config(&i2s_io_conf);
        
        // Add some delay to ensure I2S and the amplifier finish initializing
        vTaskDelay(pdMS_TO_TICKS(500));  // Increase the delay to ensure initialization completes
        
        ESP_LOGI(TAG, "Audio system initialization complete");
    }

    // Handle custom commands
    virtual bool HandleCustomCommand(const std::string& command) {
        if (command == "nod") {
            PerformNodAction();
            return true;
        } else if (command == "shake") {
            PerformShakeAction();
            return true;
        } else if (command == "happy") {
            DisplayEmotion(EMOTION_HAPPY);
            return true;
        } else if (command == "sad") {
            DisplayEmotion(EMOTION_SAD);
            return true;
        } else if (command == "surprised") {
            DisplayEmotion(EMOTION_SURPRISED);
            return true;
        } else if (command == "angry") {
            DisplayEmotion(EMOTION_ANGRY);
            return true;
        } else if (command == "neutral") {
            DisplayEmotion(EMOTION_NEUTRAL);
            return true;
        }
        
        return false;
    }

public:
    ESP32UnoD1R32Board() : boot_button_(BOOT_BUTTON_GPIO)
    {
        ESP_LOGI(TAG, "Initializing ESP32 UNO D1 R32 board...");
        
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeServos();
        InitializeAudio();  // Add audio initialization
        InitializeTouchSensor();
        InitializeButtons();
        InitializeIot();
        
        // Get the optimized audio codec
        AudioCodec* audio_codec = GetAudioCodec();
        if (audio_codec) {
            audio_codec->SetOutputVolume(AUDIO_VOLUME);
            ESP_LOGI(TAG, "Audio volume set to %d", AUDIO_VOLUME);
        }

        // Show the happy emotion after initialization completes
        DisplayEmotion(EMOTION_HAPPY);
        
        ESP_LOGI(TAG, "ESP32 UNO D1 R32 board initialization complete");
    }

    // Get the optimized audio codec
    virtual AudioCodec* GetAudioCodec() override 
    {
        static AudioCodec* cached_audio_codec = nullptr;
        
        // If the codec was already created, return the cached instance directly
        if (cached_audio_codec) {
            return cached_audio_codec;
        }
        
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        // Create the audio codec instance
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, 
            AUDIO_I2S_SPK_GPIO_LRCK, 
            AUDIO_I2S_SPK_GPIO_DOUT, 
            AUDIO_I2S_MIC_GPIO_SCK, 
            AUDIO_I2S_MIC_GPIO_WS, 
            AUDIO_I2S_MIC_GPIO_DIN
        );
        
        // Apply audio optimization
        ESP_LOGI(TAG, "Applying audio optimization settings");
        
        // Cache the codec instance
        cached_audio_codec = &audio_codec;
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        
        // Cache the codec instance
        cached_audio_codec = &audio_codec;
#endif
        return cached_audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(ESP32UnoD1R32Board);
