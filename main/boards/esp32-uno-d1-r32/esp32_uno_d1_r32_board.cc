/**
 * @file esp32_uno_d1_r32_board.cc
 * @brief ESP32 UNO D1 R32开发板实现
 * 
 * 本文件实现了ESP32 UNO D1 R32开发板的功能，包括音频输入输出、显示、舵机控制等
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

// 声明字体
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

// 舵机配置
#define LEDC_TIMER           LEDC_TIMER_0
#define LEDC_MODE            LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES        LEDC_TIMER_13_BIT  // 分辨率
#define LEDC_FREQUENCY       50                 // 50Hz PWM频率

// 舵机角度范围
#define SERVO_MIN_ANGLE      0
#define SERVO_MAX_ANGLE      180
#define SERVO_MID_ANGLE      90

// 舵机脉冲宽度范围 (单位: 微秒)
#define SERVO_MIN_PULSEWIDTH 500   // 0.5ms
#define SERVO_MAX_PULSEWIDTH 2500  // 2.5ms

// 舵机类型
typedef enum {
    SERVO_HORIZONTAL = 0,  // 水平舵机
    SERVO_VERTICAL = 1,    // 垂直舵机
    SERVO_MAX
} servo_type_t;

// 舵机通道配置
static ledc_channel_t servo_channels[SERVO_MAX] = {
    LEDC_CHANNEL_0,  // 水平舵机
    LEDC_CHANNEL_1   // 垂直舵机
};

// 舵机引脚配置
static uint8_t servo_pins[SERVO_MAX] = {
    SERVO_HORIZONTAL_PIN,  // 水平舵机
    SERVO_VERTICAL_PIN     // 垂直舵机
};

// 舵机当前角度
static uint8_t servo_angles[SERVO_MAX] = {
    SERVO_MID_ANGLE,  // 水平舵机初始角度
    SERVO_MID_ANGLE   // 垂直舵机初始角度
};

// 表情定义
typedef enum {
    EMOTION_HAPPY = 0,
    EMOTION_SAD,
    EMOTION_SURPRISED,
    EMOTION_ANGRY,
    EMOTION_NEUTRAL,
    EMOTION_MAX
} emotion_type_t;

// 表情位图数据
static const uint8_t emotion_bitmaps[][128] = {
    // 开心表情
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
    // 悲伤表情
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
    // 其他表情数据...
};

// 动作序列定义
typedef struct {
    uint8_t horizontal_angle;
    uint8_t vertical_angle;
    uint32_t delay_ms;
} servo_action_t;

// 点头动作序列
static const servo_action_t nod_sequence[] = {
    {90, 70, 300},
    {90, 110, 300},
    {90, 70, 300},
    {90, 90, 300}
};

// 摇头动作序列
static const servo_action_t shake_sequence[] = {
    {70, 90, 300},
    {110, 90, 300},
    {70, 90, 300},
    {90, 90, 300}
};

// 将角度转换为占空比
static uint32_t angle_to_duty(uint8_t angle)
{
    // 将角度映射到脉冲宽度
    uint32_t pulse_width = SERVO_MIN_PULSEWIDTH + (angle * (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) / SERVO_MAX_ANGLE);
    
    // 将脉冲宽度转换为占空比值
    uint32_t duty = (pulse_width * ((1 << LEDC_DUTY_RES) - 1)) / (1000000 / LEDC_FREQUENCY);
    
    return duty;
}

class ESP32UnoD1R32Board : public WifiBoard {
private:
    Button boot_button_;
    
    // 触摸传感器相关变量
    bool is_touch_active_ = false;
    int64_t touch_start_time_ = 0;
    bool is_recording_ = false;
    TaskHandle_t touch_task_handle_ = nullptr;

    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;

    // 初始化I2C总线
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

    // 初始化SSD1306 OLED显示屏
    void InitializeSsd1306Display() {
        // SSD1306配置
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

        // 重置显示屏
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // 打开显示屏
        ESP_LOGI(TAG, "Turning on display");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }

    // 初始化舵机
    void InitializeServos() {
        ESP_LOGI(TAG, "Initializing servos...");

        // 配置LEDC定时器
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_MODE,
            .duty_resolution = LEDC_DUTY_RES,
            .timer_num = LEDC_TIMER,
            .freq_hz = LEDC_FREQUENCY,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        // 配置LEDC通道
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

        // 初始化随机数生成器
        srand(time(NULL));

        // 将舵机移动到中间位置
        for (int i = 0; i < SERVO_MAX; i++) {
            SetServoAngle((servo_type_t)i, SERVO_MID_ANGLE);
        }

        ESP_LOGI(TAG, "Servo initialization successful");
    }

    // 设置舵机角度
    void SetServoAngle(servo_type_t type, uint8_t angle) {
        if (type >= SERVO_MAX) {
            return;
        }

        // 限制角度范围
        if (angle > SERVO_MAX_ANGLE) {
            angle = SERVO_MAX_ANGLE;
        }

        // 设置占空比
        uint32_t duty = angle_to_duty(angle);
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, servo_channels[type], duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, servo_channels[type]));

        // 更新当前角度
        servo_angles[type] = angle;
    }

    // 执行点头动作
    void PerformNodAction() {
        ESP_LOGI(TAG, "Performing nodding motion");
        
        for (int i = 0; i < sizeof(nod_sequence) / sizeof(nod_sequence[0]); i++) {
            SetServoAngle(SERVO_HORIZONTAL, nod_sequence[i].horizontal_angle);
            SetServoAngle(SERVO_VERTICAL, nod_sequence[i].vertical_angle);
            vTaskDelay(pdMS_TO_TICKS(nod_sequence[i].delay_ms));
        }
    }

    // 执行摇头动作
    void PerformShakeAction() {
        ESP_LOGI(TAG, "Performing head-shaking motion");
        
        for (int i = 0; i < sizeof(shake_sequence) / sizeof(shake_sequence[0]); i++) {
            SetServoAngle(SERVO_HORIZONTAL, shake_sequence[i].horizontal_angle);
            SetServoAngle(SERVO_VERTICAL, shake_sequence[i].vertical_angle);
            vTaskDelay(pdMS_TO_TICKS(shake_sequence[i].delay_ms));
        }
    }

    // 显示表情
    void DisplayEmotion(emotion_type_t emotion) {
        if (emotion >= EMOTION_MAX || !display_) {
            return;
        }

        // 获取当前情绪对应的文本
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
        
        // 使用Display类的公共方法显示表情
        display_->SetEmotion(emotion_text);
    }

    // 按钮配置
    #define BOOT_BUTTON_GPIO        GPIO_NUM_0   // 板载BOOT按钮（系统功能，不应修改）
    #define TOUCH_SENSOR_GPIO       GPIO_NUM_4   // 触摸传感器引脚
    #define TOUCH_THRESHOLD         1000         // 触摸阈值
    #define BUILTIN_LED_GPIO        GPIO_NUM_2   // 板载LED

    // 按钮长按时间定义（毫秒）
    #define BUTTON_LONG_PRESS_TIME  1000         // 1秒

    // 初始化触摸传感器
    void InitializeTouchSensor() {
        ESP_LOGI(TAG, "Initializing touch sensor...");
        
#if TOUCH_SENSOR_TYPE == 0
        // 使用ESP32内置电容式触摸传感器
        ESP_LOGI(TAG, "Using built-in ESP32 capacitive touch sensor (GPIO%d)", TOUCH_SENSOR_GPIO);
        
        // 初始化触摸传感器
        touch_pad_init();
        
        // 设置触摸传感器的阈值
        touch_pad_config((touch_pad_t)(TOUCH_SENSOR_GPIO - GPIO_NUM_4), TOUCH_THRESHOLD);
        
        // 创建触摸传感器监控任务
        xTaskCreate([](void* arg) {
            ESP32UnoD1R32Board* board = static_cast<ESP32UnoD1R32Board*>(arg);
            uint16_t touch_value;
            bool last_touch_state = false;
            
            while (true) {
                // 读取触摸传感器值
                touch_pad_read((touch_pad_t)(TOUCH_SENSOR_GPIO - GPIO_NUM_4), &touch_value);
                
                // 判断是否触摸（值越小表示触摸越强）
                bool current_touch_state = (touch_value < TOUCH_THRESHOLD);
                
                // 检测触摸状态变化
                if (current_touch_state != last_touch_state) {
                    if (current_touch_state) {
                        // 触摸开始
                        board->is_touch_active_ = true;
                        board->touch_start_time_ = esp_timer_get_time() / 1000;
                        gpio_set_level(BUILTIN_LED_GPIO, 1); // 点亮LED指示触摸
                        
                        // 显示中性表情，表示准备中
                        board->DisplayEmotion(EMOTION_NEUTRAL);
                        ESP_LOGI(TAG, "Touch started, preparing...");
                    } else {
                        // 触摸结束
                        board->is_touch_active_ = false;
                        int64_t current_time = esp_timer_get_time() / 1000;
                        int64_t touch_duration = current_time - board->touch_start_time_;
                        
                        gpio_set_level(BUILTIN_LED_GPIO, 0); // 关闭LED
                        
                        if (board->is_recording_) {
                            // 如果正在录音，则停止录音并发送语音
                            Application::GetInstance().StopListening();
                            board->is_recording_ = false;
                            
                            // 显示思考表情
                            board->DisplayEmotion(EMOTION_NEUTRAL);
                            ESP_LOGI(TAG, "Stopping recording and sending audio");
                        } else if (touch_duration < BUTTON_LONG_PRESS_TIME) {
                            // 短触 - 切换聊天状态
                            Application::GetInstance().ToggleChatState();
                            
                            // 执行点头动作并显示开心表情
                            board->PerformNodAction();
                            board->DisplayEmotion(EMOTION_HAPPY);
                            ESP_LOGI(TAG, "Short touch, toggling chat state");
                        }
                    }
                    
                    last_touch_state = current_touch_state;
                }
                
                // 检测长触
                if (board->is_touch_active_ && !board->is_recording_) {
                    int64_t current_time = esp_timer_get_time() / 1000;
                    int64_t touch_duration = current_time - board->touch_start_time_;
                    
                    if (touch_duration >= BUTTON_LONG_PRESS_TIME) {
                        // 长触超过设定时间，开始录音
                        board->is_recording_ = true;
                        Application::GetInstance().StartListening();
                        
                        // 显示录音表情
                        board->DisplayEmotion(EMOTION_SURPRISED);
                        ESP_LOGI(TAG, "Long touch, starting recording");
                    }
                }
                
                vTaskDelay(pdMS_TO_TICKS(50)); // 每50ms检查一次
            }
        }, "touch_monitor", 4096, this, 5, &touch_task_handle_);
#else
        // 使用外部三线式触摸开关模块（数字输入）
        ESP_LOGI(TAG, "Using external three-wire touch switch module (GPIO%d)", TOUCH_SENSOR_GPIO);
        
        // 配置GPIO为输入
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << TOUCH_SENSOR_GPIO,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
        
        // 创建触摸传感器监控任务
        xTaskCreate([](void* arg) {
            ESP32UnoD1R32Board* board = static_cast<ESP32UnoD1R32Board*>(arg);
            bool last_touch_state = false;
            
            while (true) {
                // 读取触摸传感器状态（高电平表示触摸）
                bool current_touch_state = gpio_get_level(TOUCH_SENSOR_GPIO);
                
                // 检测触摸状态变化
                if (current_touch_state != last_touch_state) {
                    if (current_touch_state) {
                        // 触摸开始
                        board->is_touch_active_ = true;
                        board->touch_start_time_ = esp_timer_get_time() / 1000;
                        gpio_set_level(BUILTIN_LED_GPIO, 1); // 点亮LED指示触摸
                        
                        // 显示中性表情，表示准备中
                        board->DisplayEmotion(EMOTION_NEUTRAL);
                        ESP_LOGI(TAG, "Touch started, preparing...");
                    } else {
                        // 触摸结束
                        board->is_touch_active_ = false;
                        int64_t current_time = esp_timer_get_time() / 1000;
                        int64_t touch_duration = current_time - board->touch_start_time_;
                        
                        gpio_set_level(BUILTIN_LED_GPIO, 0); // 关闭LED
                        
                        if (board->is_recording_) {
                            // 如果正在录音，则停止录音并发送语音
                            Application::GetInstance().StopListening();
                            board->is_recording_ = false;
                            
                            // 显示思考表情
                            board->DisplayEmotion(EMOTION_NEUTRAL);
                            ESP_LOGI(TAG, "Stopping recording and sending audio");
                        } else if (touch_duration < BUTTON_LONG_PRESS_TIME) {
                            // 短触 - 切换聊天状态
                            Application::GetInstance().ToggleChatState();
                            
                            // 执行点头动作并显示开心表情
                            board->PerformNodAction();
                            board->DisplayEmotion(EMOTION_HAPPY);
                            ESP_LOGI(TAG, "Short touch, toggling chat state");
                        }
                    }
                    
                    last_touch_state = current_touch_state;
                }
                
                // 检测长触
                if (board->is_touch_active_ && !board->is_recording_) {
                    int64_t current_time = esp_timer_get_time() / 1000;
                    int64_t touch_duration = current_time - board->touch_start_time_;
                    
                    if (touch_duration >= BUTTON_LONG_PRESS_TIME) {
                        // 长触超过设定时间，开始录音
                        board->is_recording_ = true;
                        Application::GetInstance().StartListening();
                        
                        // 显示录音表情
                        board->DisplayEmotion(EMOTION_SURPRISED);
                        ESP_LOGI(TAG, "Long touch, starting recording");
                    }
                }
                
                vTaskDelay(pdMS_TO_TICKS(50)); // 每50ms检查一次
            }
        }, "touch_monitor", 4096, this, 5, &touch_task_handle_);
#endif
        
        ESP_LOGI(TAG, "Touch sensor initialization complete");
    }

    // 初始化按钮
    void InitializeButtons() {
        // 配置GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,  // 设置需要配置的GPIO引脚
            .mode = GPIO_MODE_OUTPUT,           // 设置为输出模式
            .pull_up_en = GPIO_PULLUP_DISABLE,  // 禁用上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁用下拉
            .intr_type = GPIO_INTR_DISABLE      // 禁用中断
        };
        gpio_config(&io_conf);  // 应用配置

        // BOOT按钮点击事件 - 仅用于WiFi配置重置，不干扰系统功能
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
                ESP_LOGI(TAG, "Resetting Wi-Fi configuration");
            }
        });
    }

    // 物联网初始化，添加对AI可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

    // 初始化音频系统
    void InitializeAudio() {
        ESP_LOGI(TAG, "Initializing audio system...");
        
        // 配置MAX98357A放大器的GPIO
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << AUDIO_I2S_SPK_GPIO_DOUT);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // 启用上拉电阻，提高信号稳定性
        gpio_config(&io_conf);
        
        // 配置I2S引脚
        gpio_config_t i2s_io_conf = {};
        i2s_io_conf.intr_type = GPIO_INTR_DISABLE;
        i2s_io_conf.mode = GPIO_MODE_OUTPUT;
        i2s_io_conf.pin_bit_mask = (1ULL << AUDIO_I2S_SPK_GPIO_BCLK) | (1ULL << AUDIO_I2S_SPK_GPIO_LRCK);
        i2s_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        i2s_io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // 启用上拉电阻，提高信号稳定性
        gpio_config(&i2s_io_conf);
        
        // 增加一些延迟，确保I2S和放大器初始化完成
        vTaskDelay(pdMS_TO_TICKS(500));  // 增加延迟时间，确保初始化完全
        
        ESP_LOGI(TAG, "Audio system initialization complete");
    }

    // 处理自定义命令
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
        InitializeAudio();  // 添加音频初始化
        InitializeTouchSensor();
        InitializeButtons();
        InitializeIot();
        
        // 获取优化后的音频编解码器
        AudioCodec* audio_codec = GetAudioCodec();
        if (audio_codec) {
            audio_codec->SetOutputVolume(AUDIO_VOLUME);
            ESP_LOGI(TAG, "Audio volume set to %d", AUDIO_VOLUME);
        }

        // 初始化完成后显示开心表情
        DisplayEmotion(EMOTION_HAPPY);
        
        ESP_LOGI(TAG, "ESP32 UNO D1 R32 board initialization complete");
    }

    // 获取优化后的音频编解码器
    virtual AudioCodec* GetAudioCodec() override 
    {
        static AudioCodec* cached_audio_codec = nullptr;
        
        // 如果已经创建过编解码器，直接返回缓存的实例
        if (cached_audio_codec) {
            return cached_audio_codec;
        }
        
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        // 创建音频编解码器实例
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
        
        // 应用音频优化
        ESP_LOGI(TAG, "Applying audio optimization settings");
        
        // 缓存编解码器实例
        cached_audio_codec = &audio_codec;
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        
        // 缓存编解码器实例
        cached_audio_codec = &audio_codec;
#endif
        return cached_audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(ESP32UnoD1R32Board);
