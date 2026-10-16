#include "wifi_board.h"
#include "audio/codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "board_config.h"
#include "sh1106_panel.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "emoji_controller.h"
#include "servo_controller.h"
#include "emotion_response_controller.h"
#include "robot_eyes.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cstring>   // Include the cstring header for strchr()
#include <stdlib.h>  // Include the standard library header for rand()
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <driver/i2c.h>
#include <driver/i2c_master.h>  // Include the new I2C driver API header
#include <driver/gpio.h>
#include <esp_attr.h>           // RTC_NOINIT_ATTR for the restart counter below
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <lvgl.h>
#include <time.h>
#include <sys/time.h>

#define TAG "EmojiBoard"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

// Declare a static function used to process the AI response
static void ProcessAIResponseTask(void* arg);

// Forward declaration of the EmojiBoard class
class EmojiBoard;

// Global variable used to store and access the EmojiBoard instance
EmojiBoard* g_board_instance = nullptr;

// Supervisor restart counter, kept in RTC slow memory so it survives esp_restart()
// (but not a power loss): this is what stops a wedged display from reboot-looping.
// RTC slow memory is garbage after a power loss, hence the magic word validation.
static RTC_NOINIT_ATTR uint32_t supervisor_magic_;
static RTC_NOINIT_ATTR uint32_t supervisor_restarts_;

// Task function declaration
static void StateMonitorTask(void* arg);

// Task parameter struct used to pass arguments to the static task functions
struct TaskParams {
    EmojiBoard* board;  // Use EmojiBoard* instead of void*
    EmotionResponseController* emotion_controller;
};

// Custom OledDisplay subclass that captures the AI reply and triggers expressions and motions
class EmojiDisplay : public OledDisplay {
private:
    EmojiBoard* board_ = nullptr;
    bool processing_ai_response_ = false; // Flag added to prevent recursive calls

public:
    // Override SetEmotion: trigger our own expression animation from the emotion recognised by the Xiaozhi AI framework
    virtual void SetEmotion(const char* emotion) override;
    
    EmojiDisplay(EmojiBoard* board, esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel, 
                int width, int height, bool flip_x, bool flip_y, 
                DisplayFonts fonts) 
        : OledDisplay(io, panel, width, height, flip_x, flip_y, fonts), board_(board) {
        ESP_LOGI(TAG, "Created EmojiDisplay instance");
    }

    // Override SetChatMessage: also trigger expressions and motions while displaying the AI reply
    void SetChatMessage(const char* role, const char* content) override;
};

class EmojiBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    // True while the panel is in sleep mode (screen dark). The eyes render task checks
    // this and skips drawing entirely: pushing pixels at a sleeping panel is pure I2C
    // traffic, and saturating the bus is what used to stall the LVGL flush for seconds
    // (task watchdog + a frozen state machine on top).
    bool panel_asleep_ = false;
    // Heartbeat published by the eyes render task after every completed frame. The
    // display supervisor below watches it: the panel I/O submits I2C transfers with
    // timeout -1 (wait forever), so electrical noise on the bus can hang a flush
    // permanently - and that hangs every task that touches LVGL, including the state
    // machine (stuck green LED, deaf to the wake word).
    volatile TickType_t display_beat_ = 0;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    
    // Expression and servo controllers
    EmojiController* emoji_controller_ = nullptr;
    ServoController* servo_controller_ = nullptr;
    
    // Emotion response controller
    EmotionResponseController* emotion_controller_ = nullptr;

    // OliEyes engine (own canvas; emoji_controller no longer draws the eyes)
    RobotEyes* eyes_ = nullptr;
    lv_obj_t* eyes_screen_ = nullptr;
    
    // Expression-mode flag
    bool is_emoji_mode_ = true;  // boot straight into expressive mode (two big eyes)

    // True once Application::Start() has built the display UI: only then is it safe to
    // create/load the eye screen (see the comment on the deferred init in the ctor)
    bool ui_ready_ = false;
    
    // Chat-mode screen
    lv_obj_t* chat_screen_ = nullptr;
    
    // Last processed AI reply
    std::string last_ai_response_;
    
    // Method that processes the AI reply
    void ProcessAIResponseInternal(const char* message) {
        // Do nothing when the message is empty
        if (!message || message[0] == '\0') {
            return;
        }
        
        ESP_LOGI(TAG, "Processing AI response: %s", message);
        
        // Skip when it is identical to the previous reply
        if (last_ai_response_ == message) {
            return;
        }
        
        // Record the last processed AI reply
        last_ai_response_ = message;
        
        // Check for a special character marker and handle it immediately when present
        if (message[0] && strchr("{}<>/\\$!?^*#~", message[0]) != nullptr) {
            ESP_LOGI(TAG, "Detected special character marker: %c", message[0]);
            emotion_controller_->ProcessAIResponse(message);
        } else {
            // No special character marker: process the AI reply normally
            emotion_controller_->ProcessAIResponse(message);
        }
    }

    void InitializeDisplayI2c() {
        // Use the new I2C driver API from ESP-IDF v5.3.2
        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = I2C_NUM_0;
        bus_config.sda_io_num = DISPLAY_I2C_SDA_PIN;
        bus_config.scl_io_num = DISPLAY_I2C_SCL_PIN;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;
        
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
        ESP_LOGI(TAG, "I2C bus initialized successfully");
    }

    void InitializeSsd1306Display() {
        esp_lcd_panel_io_i2c_config_t io_config = {};
        io_config.dev_addr = DISPLAY_I2C_ADDR;
        io_config.on_color_trans_done = nullptr;
        io_config.user_ctx = nullptr;
        io_config.control_phase_bytes = 1;
        io_config.dc_bit_offset = 6;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.flags.dc_low_on_data = 0;
        io_config.flags.disable_control_phase = 0;
        io_config.scl_speed_hz = 100 * 1000;

        // Use the new I2C panel IO API
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));
        ESP_LOGI(TAG, "LCD panel I/O initialized successfully");

        ESP_LOGI(TAG, "Installing SH1106-compatible page-addressing driver");
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, -1, &panel_));
        ESP_LOGI(TAG, "SH1106-compatible driver installed successfully");

        // The very first I2C transaction after power-up sometimes fails (bus noise
        // while the chip rails settle), which used to give up immediately and leave
        // the robot permanently screenless (NoDisplay) until the next power cycle -
        // showing a frozen stale frame. Retry the reset+init sequence instead.
        static constexpr int kPanelInitRetries = 5;
        esp_err_t err = ESP_FAIL;
        for (int attempt = 1; attempt <= kPanelInitRetries; ++attempt) {
            ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
            err = esp_lcd_panel_init(panel_);
            if (err == ESP_OK) {
                break;
            }
            ESP_LOGW(TAG, "Display init attempt %d/%d failed (%s), retrying",
                     attempt, kPanelInitRetries, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        // Create the DisplayFonts object
        DisplayFonts display_fonts;
        display_fonts.text_font = &font_puhui_14_1;
        display_fonts.icon_font = &font_awesome_14_1;

        // Use the custom EmojiDisplay class
        display_ = new EmojiDisplay(this, panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, true, true, display_fonts);

        // Keep a reference to the chat-mode screen
        chat_screen_ = lv_screen_active();
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            // A short press of the BOOT button always starts recording, in any mode
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        boot_button_.OnLongPress([this]() {
            if (is_emoji_mode_) {
                ExitEmojiMode(true);
            } else {
                EnterEmojiMode(true);
            }
        });

        volume_up_button_.OnClick([this]() {
            // The volume-up button always increases the volume, in any mode
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            // Long-pressing volume-up always sets the volume to maximum, in any mode
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            // The volume-down button always decreases the volume, in any mode
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            // Long-pressing volume-down always sets the volume to 0, in any mode
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    // Five head tools for the assistant: look left/right, nod, shake, centre.
    // They reuse the existing emotion pipeline (TriggerEmotion -> PlayAnimation/servo),
    // which is LVGL-safe and does not collide with running animations.
    void RegisterHeadTools() {
        auto& mcp = McpServer::GetInstance();
        EmotionResponseController* emotion = emotion_controller_;
        if (emotion == nullptr) {
            ESP_LOGW(TAG, "Emotion controller is null; skip head tools registration");
            return;
        }

        mcp.AddTool("self.head.look_left",
            "Turn the robot head to its left and look that way.",
            PropertyList(),
            [emotion](const PropertyList&) -> ReturnValue {
                emotion->TriggerEmotion("look_left");
                return true;
            });

        mcp.AddTool("self.head.look_right",
            "Turn the robot head to its right and look that way.",
            PropertyList(),
            [emotion](const PropertyList&) -> ReturnValue {
                emotion->TriggerEmotion("look_right");
                return true;
            });

        mcp.AddTool("self.head.nod",
            "Move the head up and down (nodding): agreement, greeting or saying yes.",
            PropertyList(),
            [emotion](const PropertyList&) -> ReturnValue {
                emotion->TriggerEmotion("nod");
                return true;
            });

        mcp.AddTool("self.head.shake",
            "Move the head left and right (shaking): disagreement, refusal or saying no.",
            PropertyList(),
            [emotion](const PropertyList&) -> ReturnValue {
                emotion->TriggerEmotion("shake");
                return true;
            });

        mcp.AddTool("self.head.center",
            "Return the head to the straight, neutral position.",
            PropertyList(),
            [emotion](const PropertyList&) -> ReturnValue {
                emotion->TriggerEmotion("look_center");
                return true;
            });

        mcp.AddTool("self.eyes.set_style",
            std::string("Change the robot's eye expression (mood). Available styles: ") +
                RobotEyes::StyleNames(),
            PropertyList({
                Property("style", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string style = properties["style"].value<std::string>();
                if (eyes_ == nullptr || !RobotEyes::IsValidStyle(style.c_str())) {
                    return std::string("unknown style, available: ") + RobotEyes::StyleNames();
                }
                DisplayLockGuard lock(display_);
                eyes_->SetMood(style.c_str());
                return true;
            });

        mcp.AddTool("self.eyes.gesture",
            "Trigger an eye gesture: blink, wink, winkleft, doubletake (startled glance + blink) "
            "or demo (resume automatic mood cycling).",
            PropertyList({
                Property("gesture", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string g = properties["gesture"].value<std::string>();
                std::transform(g.begin(), g.end(), g.begin(), [](unsigned char c){ return std::tolower(c); });
                if (eyes_ == nullptr) return false;
                DisplayLockGuard lock(display_);
                if (g == "blink") { eyes_->Blink(); return true; }
                if (g == "wink") { eyes_->Wink(true); return true; }
                if (g == "winkleft") { eyes_->Wink(false); return true; }
                if (g == "doubletake") { eyes_->DoubleTake(true); return true; }
                if (g == "demo") { eyes_->SetDemo(true); return true; }
                return std::string("unknown gesture, use: blink, wink, winkleft, doubletake, demo");
            });

        ESP_LOGI(TAG, "Registered 5 head tools + self.eyes.set_style + self.eyes.gesture");
    }

    void InitializeIot() {
        // The new MCP architecture no longer needs manual Thing initialisation; the framework manages it automatically
        ESP_LOGI(TAG, "Device functions are managed automatically by the new MCP architecture");
        RegisterHeadTools();
    }
    
    // Switch the screen
    // Create the screen for the OliEyes engine (white background / black shapes,
    // which renders as a dark background with lit eyes on this inverted panel)
    void CreateEyesScreen() {
        DisplayLockGuard lock(display_);
        eyes_screen_ = lv_obj_create(nullptr);
        lv_obj_set_size(eyes_screen_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(eyes_screen_, lv_color_white(), 0);
        lv_obj_set_style_border_width(eyes_screen_, 0, 0);
        eyes_ = new RobotEyes();
        eyes_->Initialize(eyes_screen_);
    }

    // Eye rendering task at ~10 fps: plenty smooth for a 128x64 OLED, and 3x less I2C
    // traffic than 30 fps. Each full-screen flush moves ~1 KB over a 100 kHz bus
    // (~100 ms); at 30 fps the flushes back up, LVGL waits with the display lock held
    // for seconds (DisplayLockGuard blocks up to 30 s), the main task's state changes
    // stall behind it (stuck green "speaking", deaf to the wake word) and the task
    // watchdog barks. While the panel sleeps, skip drawing altogether.
    void StartEyesTask() {
        xTaskCreate([](void* arg) {
            EmojiBoard* self = static_cast<EmojiBoard*>(arg);
            while (true) {
                // Mic-first exclusivity: while the robot is speaking, the speaker draws
                // peak current (the prime suspect for I2C glitches) and nobody watches
                // the face anyway - so freeze the eyes and keep the bus silent.
                // Listening keeps animating (the boss is looking at it).
                if (self->eyes_ != nullptr && !self->panel_asleep_ &&
                    Application::GetInstance().GetDeviceState() != kDeviceStateSpeaking) {
                    // Best-effort: when a display flush is hung the LVGL lock stays
                    // held, and blocking here would stall eye animation behind it.
                    // Skip the frame instead; the next one follows in 100 ms.
                    DisplayTryLockGuard lock(self->display_);
                    if (!lock.locked()) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                        continue;
                    }
                    // In eye mode the eye screen must stay visible: if anything switched
                    // the screen away, take it back (logged, so a thief is easy to spot).
                    if (self->is_emoji_mode_ && self->eyes_screen_ &&
                        lv_screen_active() != self->eyes_screen_) {
                        lv_scr_load(self->eyes_screen_);
                        ESP_LOGW(TAG, "eye screen was not active - reloaded it");
                    }
                    self->eyes_->Step();
                    // Publish the heartbeat only after a full frame made it out, so the
                    // supervisor below can tell "rendering fine" from "flush is hung".
                    self->display_beat_ = xTaskGetTickCount();
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }, "eyes_render", 4096, this, 2, nullptr);
    }

    // Display supervisor: the SH1106 panel driver submits I2C transfers with timeout
    // -1 (wait forever), so a single noisy bus transaction hangs the LVGL flush, and
    // with it every task that wants the display lock - the state machine goes deaf and
    // the LED freezes. The eyes task publishes display_beat_ after each frame; if that
    // heartbeat goes quiet for 8 s the bus is hung. Reset it (i2c_master_bus_reset).
    // A chip restart is the last resort and is deliberately rate-limited: restarting
    // into the same wedge every 30 s (reboot loop) is worse than a frozen screen, so
    // at most 2 supervisor restarts per power cycle, and never within the first 4
    // minutes of uptime (early wedges are retried with bus resets only).
    void StartDisplaySupervisorTask() {
        xTaskCreate([](void* arg) {
            EmojiBoard* self = static_cast<EmojiBoard*>(arg);
            // Validate the RTC counter (garbage after power loss).
            if (supervisor_magic_ != 0xC0FFEE42) {
                supervisor_magic_ = 0xC0FFEE42;
                supervisor_restarts_ = 0;
            }
            bool reset_attempted = false;
            while (true) {
                vTaskDelay(pdMS_TO_TICKS(3000));
                if (self->panel_asleep_) {          // nothing is being drawn: no stall
                    reset_attempted = false;
                    continue;
                }
                if (self->display_beat_ == 0) {     // renderer never ran yet: booting
                    continue;
                }
                const TickType_t silence =
                    xTaskGetTickCount() - self->display_beat_;
                if (silence < pdMS_TO_TICKS(8000)) { // rendering fine
                    reset_attempted = false;
                    // A long healthy stretch forgives past restarts.
                    if (silence < pdMS_TO_TICKS(1000) &&
                        xTaskGetTickCount() > pdMS_TO_TICKS(600000)) {
                        supervisor_restarts_ = 0;
                    }
                    continue;
                }
                ESP_LOGE(TAG, "Display stalled: no frame for %u ms (bus is hung)",
                         (unsigned)silence);
                if (self->display_i2c_bus_ == nullptr) {
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    continue;
                }
                if (!reset_attempted) {
                    ESP_LOGW(TAG, "Resetting the display I2C bus to recover");
                    esp_err_t err = i2c_master_bus_reset(self->display_i2c_bus_);
                    ESP_LOGW(TAG, "i2c_master_bus_reset -> %s", esp_err_to_name(err));
                    // A bus reset only fixes the master side. If the SH1106 itself
                    // crashed (controller hung mid-byte holding SDA), only a fresh
                    // init sequence revives it - so re-run one, fire-and-forget: these
                    // calls can block on the wedged bus too, and the supervisor must
                    // keep watching instead of hanging with them.
                    xTaskCreate([](void* arg) {
                        EmojiBoard* self = static_cast<EmojiBoard*>(arg);
                        if (self->panel_ == nullptr) {
                            vTaskDelete(nullptr);
                        }
                        ESP_LOGW(TAG, "Re-init worker: resetting the panel");
                        if (esp_lcd_panel_reset(self->panel_) != ESP_OK ||
                            esp_lcd_panel_init(self->panel_) != ESP_OK ||
                            esp_lcd_panel_disp_on_off(self->panel_, true) != ESP_OK) {
                            ESP_LOGE(TAG, "Re-init worker: panel did not come back");
                        } else {
                            ESP_LOGW(TAG, "Re-init worker: panel back on");
                        }
                        vTaskDelete(nullptr);
                    }, "panel_reinit", 4096, self, 1, nullptr);
                    reset_attempted = true;
                    // A reset aborts the in-flight transfer, so the flush should return
                    // and the heartbeat resume within a few seconds.
                    vTaskDelay(pdMS_TO_TICKS(6000));
                    continue;
                }
                // Still silent after a bus reset: restart, but rate-limited (see above).
                if (xTaskGetTickCount() > pdMS_TO_TICKS(240000) &&
                    supervisor_restarts_ < 2) {
                    ESP_LOGE(TAG, "Display still stalled after a bus reset - restarting");
                    supervisor_restarts_++;
                    vTaskDelay(pdMS_TO_TICKS(500));  // let the log reach the port
                    esp_restart();
                }
                ESP_LOGE(TAG, "Display wedged; restart withheld (loop protection) - "
                              "power-cycle the board and check the 3V3 rail / USB supply");
                vTaskDelay(pdMS_TO_TICKS(30000));
            }
        }, "disp_super", 4096, this, 1, nullptr);
    }

    // Enter / leave the expressive-eyes mode - shared by the BOOT long press and startup
    void EnterEmojiMode(bool show_notification) {
        is_emoji_mode_ = true;
        if (!eyes_screen_ && ui_ready_) {
            CreateEyesScreen();
        }
        SwitchScreen(true);
        emoji_controller_->StartBlinkTimer();
        emoji_controller_->EyeCenter();
        if (servo_controller_) {
            servo_controller_->HeadCenter();
        }
        if (show_notification) {
            GetDisplay()->ShowNotification("Chế độ biểu cảm");
        }
    }

    void ExitEmojiMode(bool show_notification) {
        is_emoji_mode_ = false;
        emoji_controller_->StopBlinkTimer();
        SwitchScreen(false);
        if (show_notification) {
            GetDisplay()->ShowNotification("Chế độ trò chuyện");
        }
        emoji_controller_->CleanupEmojiScreen();
        if (servo_controller_) {
            servo_controller_->HeadCenter();
        }
    }

    void SwitchScreen(bool to_emoji_mode) {
        // Servo first (needs no display lock), screen load best-effort: skipping a
        // screen switch under contention is harmless, blocking 30 s is not.
        if (to_emoji_mode) {
            if (servo_controller_) {
                servo_controller_->HeadCenter();
            }
        }
        DisplayTryLockGuard lock(display_);
        if (!lock.locked()) {
            return;
        }
        if (to_emoji_mode) {
            // Still create the emoji screen so the old animation/servo system keeps running (it does not drive the servo),
            // but the VISIBLE screen is the OliEyes canvas.
            lv_obj_t* emoji_screen = emoji_controller_->GetEmojiScreen();
            if (!emoji_screen) {
                emoji_screen = emoji_controller_->CreateEmojiScreen();
            }
            lv_scr_load(eyes_screen_ ? eyes_screen_ : emoji_screen);
        } else {
            lv_scr_load(chat_screen_);
        }
    }

    // Declare the static task function as a friend so it can access private members
    friend void StateMonitorTask(void* arg);

    // Declare EmojiDisplay as a friend class so it can access EmojiBoard's private members
    friend class EmojiDisplay;

    // Declare ProcessAIResponseTask as a friend function so it can access private members
    friend void ProcessAIResponseTask(void* arg);

public:
    EmojiBoard() :
        boot_button_(BOOT_BUTTON_PIN),
        volume_up_button_(VOLUME_UP_BUTTON_PIN),
        volume_down_button_(VOLUME_DOWN_BUTTON_PIN) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        
        // Create the expression controller and the servo controller
        emoji_controller_ = new EmojiController(display_);
        servo_controller_ = new ServoController();
        
        // Initialise the controllers
        emoji_controller_->Initialize();
        servo_controller_->Initialize();
        
        // Give the expression controller its servo controller
        emoji_controller_->SetServoController(servo_controller_);
        
        // Create and initialise the emotion response controller
        emotion_controller_ = new EmotionResponseController(emoji_controller_, servo_controller_, GetAudioCodec());
        emotion_controller_->Initialize();
        
        // Gesture recognition has been removed
        ESP_LOGI(TAG, "Gesture recognition has been removed");
        
        // The new MCP architecture no longer needs a global emotion controller pointer
        // iot::SetGlobalEmotionController(emotion_controller_);
        
        InitializeButtons();
        InitializeIot();
        
        // Boot straight into the expressive-eyes mode and stay there.
        // The BOOT long press still toggles with the chat screen.
        //
        // The eye screen must NOT be created here. Application::Start() calls
        // Display::SetupUI() a few hundred ms later and builds the chat container on
        // whichever screen is active AT THAT MOMENT - loading the eye screen this early
        // makes SetupUI parent its opaque, full-size container to the eye screen, which
        // then covers the canvas and leaves the OLED looking blank. So: start the
        // renderer now, create and load the eye screen once the UI exists.
        StartEyesTask();
        StartDisplaySupervisorTask();
        xTaskCreate([](void* arg) {
            EmojiBoard* self = static_cast<EmojiBoard*>(arg);
            vTaskDelay(pdMS_TO_TICKS(3000));   // let Application::Start()/SetupUI finish
            self->ui_ready_ = true;
            self->EnterEmojiMode(false);
            // Type-B idle face (calm): NO demo mood cycling - the mood stays put and only
            // blinks (2-6 s) and occasional glances move. Combined with render-on-change
            // below, a still face generates zero I2C traffic, which is the whole point.
            ESP_LOGI(TAG, "Emoji (eyes) mode is ON at startup (esp32-eyes style)");
            vTaskDelete(nullptr);
        }, "eyes_init", 4096, this, 2, nullptr);
        
        // Create a task that monitors device state changes
        TaskParams* state_params = new TaskParams();
        state_params->emotion_controller = emotion_controller_;
        state_params->board = this;
        xTaskCreate(StateMonitorTask, "StateMonitor", 8192, state_params, 1, NULL);
        
        // Store this instance in the global variable
        g_board_instance = this;
    }

    ~EmojiBoard() {
        delete emoji_controller_;
        delete servo_controller_;
        delete emotion_controller_;
        // Clear the global variable
        g_board_instance = nullptr;
    }

    virtual Led* GetLed() override {
        static SingleLed led(LED_PIN);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            I2S_SPEAKER_BCLK_PIN, I2S_SPEAKER_WS_PIN, I2S_DATA_OUT_PIN,
            I2S_MIC_SCK_PIN, I2S_MIC_WS_PIN, I2S_DATA_IN_PIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    /**
     * @brief Process a command entered by the user
     * @param message The message entered by the user
     */
    void ProcessUserCommand(const char* message) {
        if (!message || message[0] == '\0') {
            return;
        }
        
        // Hand the user command straight to the emotion response controller
        if (emotion_controller_) {
            // Check whether it is an expression-motion command
            if (emotion_controller_->ProcessEmotionCommand(message)) {
                ESP_LOGI(TAG, "Processed user-entered expression action command: %s", message);
                return;
            }
            
            // Check whether it is a volume control command
            if (emotion_controller_->ProcessVolumeCommand(message)) {
                ESP_LOGI(TAG, "Processed user-entered volume control command: %s", message);
                return;
            }
        }
    }

    // Public method that processes the AI reply; may be called from outside
    void ProcessAIResponse(const char* message) {
        ProcessAIResponseInternal(message);
    }
};

// Implement EmojiDisplay::SetChatMessage after the EmojiBoard class definition
void EmojiDisplay::SetChatMessage(const char* role, const char* content) {
    // Guard against recursive calls while an AI reply is being processed
    if (processing_ai_response_) {
        // While an AI reply is being processed, only call the base method to display the message; do no extra work
        OledDisplay::SetChatMessage(role, content);
        return;
    }
    
    // Set the flag indicating an AI reply is being processed
    processing_ai_response_ = true;
    
    // First call the base method to display the message
    OledDisplay::SetChatMessage(role, content);
    
    // If this is an AI reply, process its content
    if (role && strcmp(role, "assistant") == 0 && content && content[0] != '\0') {
        ESP_LOGI(TAG, "EmojiDisplay captured AI response: %s", content);
        
        // Call EmojiBoard::ProcessAIResponse to handle the AI reply
        if (board_ && board_->emotion_controller_) {
            // Simplified handling: only update last_ai_response_ and call ProcessAIResponse
            if (board_->last_ai_response_ != content) {
                board_->last_ai_response_ = content;
                
                // Check for a special character marker and handle it directly when present
                if (content[0] && strchr("{}<>/\\$!?^*#~", content[0]) != nullptr) {
                    ESP_LOGI(TAG, "Detected special character marker: %c", content[0]);
                    
                    // Pass the special character straight to the emotion controller
                    board_->emotion_controller_->ProcessAIResponse(content);
                    
                    // No extra task is created for the AI reply here, to avoid a stack overflow
                } else {
                    // Handle the AI reply in a background task, to keep complex work out of the main loop
                    // Increase the stack size to avoid a stack overflow
                    xTaskCreate(ProcessAIResponseTask, "ai_response", 8192, strdup(content), 1, NULL);
                }
            }
        }
    }
    
    // Reset the flag
    processing_ai_response_ = false;
}

// Static task function that monitors device state
static void StateMonitorTask(void* arg) {
    TaskParams* params = static_cast<TaskParams*>(arg);
    EmojiBoard* board = params->board;
    if (!board) {
        ESP_LOGE(TAG, "Board instance is null");
        vTaskDelete(nullptr);
        return;
    }
    
    // Get the emotion controller
    auto* emotion_controller = params->emotion_controller;
    if (!emotion_controller) {
        ESP_LOGE(TAG, "Emotion controller is null");
        vTaskDelete(nullptr);
        return;
    }
    
    // Initialise the random number generator
    srand(time(nullptr));
    
    // Initialise the variables
    DeviceState last_state = kDeviceStateIdle;
    TickType_t last_speak_end_time = 0;
    bool in_conversation = false;

    // --- Context-aware behaviour ---
    // idle -> (6s of silence) bored -> (20s) suspicious -> (120s) sleepy eyes ->
    // (180s) panel asleep (screen off); any wake word / button press leaves idle and
    // wakes the screen back up. While listening -> curious + head up
    TickType_t idle_since = xTaskGetTickCount();
    bool idle_bored_done = false;
    bool idle_suspicious_done = false;
    bool idle_sleepy_done = false;
    bool idle_asleep_done = false;
    // Panel sleep state lives on the board (panel_asleep_) so the eyes render task can
    // see it too; no local copy here.

    auto set_eyes_mood = [&](const char* mood) {
        if (board->eyes_ == nullptr) return;
        // No lock: SetMood only writes an int consumed by the render task (benign).
        board->eyes_->SetMood(mood);
    };
    auto set_eyes_idle = [&](bool on) {
        if (board->eyes_ == nullptr) return;
        // No lock: SetIdleBehaviors only flips a bool (benign).
        board->eyes_->SetIdleBehaviors(on);   // turn random eye glances/blinks on/off plus demo mode
    };
    
    // Make sure random animation is enabled in the initial state
    if (board->emoji_controller_) {
        board->emoji_controller_->SetRandomAnimationEnabled(true);
        ESP_LOGI(TAG, "Initialization: random expression animation enabled");
    }
    
    // Monitor the device state
    while (true) {
        // Get the current device state
        DeviceState current_state = Application::GetInstance().GetDeviceState();
        
        // Log the device state when it changes
        if (current_state != last_state) {
            ESP_LOGI(TAG, "Device state changed: %d -> %d", last_state, current_state);
            // Leaving idle (wake word, button, ...) while the panel is asleep means
            // the boss is back: power the screen up again before anything else draws.
            if (last_state == kDeviceStateIdle && board->panel_asleep_ && board->panel_ != nullptr) {
                if (esp_lcd_panel_disp_on_off(board->panel_, true) == ESP_OK) {
                    board->panel_asleep_ = false;
                    ESP_LOGI(TAG, "Woke up: panel back on");
                } else {
                    ESP_LOGW(TAG, "Woke up but the panel would not turn back on");
                }
            }
        }
        
        // A transition from idle to speaking or listening means a conversation has started
        if (last_state == kDeviceStateIdle && 
            (current_state == kDeviceStateSpeaking || current_state == kDeviceStateListening)) {
            // Mark as being in a conversation
            in_conversation = true;
            
            // Stop the random expression animation
            if (board->emoji_controller_) {
                board->emoji_controller_->SetRandomAnimationEnabled(false);
                board->emoji_controller_->ClearAnimationQueue();
                ESP_LOGI(TAG, "Conversation started; stopping random expression animation");
            }
            
            // Context-aware reaction (replaces the old random happy/surprised emotion)
            set_eyes_idle(false);
            if (current_state == kDeviceStateListening) {
                set_eyes_mood("curious");
                if (board->servo_controller_ != nullptr) {
                    board->servo_controller_->HeadUp(10);   // tilt the head slightly up: "I am listening to the boss"
                }
                ESP_LOGI(TAG, "Listening: eyes=curious, head slightly up");
            } else if (current_state == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "Speaking: eyes/head follow the assistant emotion");
            }
        }
        // Speaking/listening without coming from idle means the conversation is continuing
        else if ((current_state == kDeviceStateSpeaking || current_state == kDeviceStateListening) && 
                 !in_conversation) {
            // Mark as being in a conversation
            in_conversation = true;
            
            // Stop the random expression animation
            if (board->emoji_controller_) {
                board->emoji_controller_->SetRandomAnimationEnabled(false);
                board->emoji_controller_->ClearAnimationQueue();
                ESP_LOGI(TAG, "Conversation continued; stopping random expression animation");
            }
            set_eyes_idle(false);
            if (current_state == kDeviceStateListening) {
                set_eyes_mood("curious");
                if (board->servo_controller_ != nullptr) {
                    board->servo_controller_->HeadUp(10);
                }
            }
        }
        
        // A transition from speaking to idle means the AI has just finished replying
        if (last_state == kDeviceStateSpeaking && current_state == kDeviceStateIdle) {
            // Record the time the AI reply ended
            last_speak_end_time = xTaskGetTickCount();
            
            // Get the most recent AI reply content
            const std::string& ai_response = board->last_ai_response_;
            
            // When there is AI reply content, derive the emotion from it
            if (!ai_response.empty()) {
                ESP_LOGI(TAG, "AI response ended; analyzing emotion from content: %s", ai_response.c_str());
                
                // Process the AI reply content directly to trigger the matching expression and motions
                emotion_controller->ProcessAIResponse(ai_response);
            } else {
                // With no AI reply content, fall back to a random emotion
                static const char* emotions[] = {
                    "happy", "sad", "surprise", "confused", "neutral", "look_left", "look_right"
                };
                int random_index = rand() % (sizeof(emotions) / sizeof(emotions[0]));
                emotion_controller->TriggerEmotion(emotions[random_index]);
                
                ESP_LOGI(TAG, "AI response ended with no content; using random emotion: %s", emotions[random_index]);
            }
        }
        
        // A transition from listening to idle means the user has just finished speaking
        if (last_state == kDeviceStateListening && current_state == kDeviceStateIdle) {
            // Try to process an expression-motion command from the user input
            // Note: the user input text is not directly available here because the framework exposes no such API
            // We can, however, check for common expression-motion commands before the AI replies
            
            // Emulate handling a few common expression-motion commands
            static const std::vector<std::string> common_commands = {
                "向左看", "向右看", "看左边", "看右边", "左看", "右看",
                "开心", "笑一笑", "高兴", "笑", "微笑",
                "悲伤", "伤心", "难过", "哭",
                "惊讶", "吃惊", "惊喜",
                "困惑", "疑惑", "迷惑",
                "正常", "平静", "中性", "恢复"
            };
            
            for (const auto& cmd : common_commands) {
                if (emotion_controller->ProcessEmotionCommand(cmd)) {
                    ESP_LOGI(TAG, "User input ended; attempting to process common expression action command: %s", cmd.c_str());
                    break;
                }
            }
        }
        
        // A transition from listening or speaking to idle means the conversation may have ended
        if ((last_state == kDeviceStateListening || last_state == kDeviceStateSpeaking) && 
            current_state == kDeviceStateIdle) {
            TickType_t current_time = xTaskGetTickCount();
            
            // Treat the conversation as ended once 3 seconds have passed
            if (in_conversation && 
                ((current_time - last_speak_end_time) > pdMS_TO_TICKS(3000))) {
                in_conversation = false;
                
                // Resume the random expression animation
                if (board->emoji_controller_) {
                    board->emoji_controller_->SetRandomAnimationEnabled(true);
                    ESP_LOGI(TAG, "Conversation ended; resuming random expression animation");
                }
                if (board->eyes_) {
                    DisplayLockGuard eyes_lock(board->display_);
                    board->eyes_->SetIdleBehaviors(true);
                    board->eyes_->SetMood("relaxed");
                }
                idle_since = xTaskGetTickCount();
                idle_bored_done = false;
                idle_suspicious_done = false;
                idle_sleepy_done = false;
                idle_asleep_done = false;
                
                // Trigger the neutral emotion
                emotion_controller->TriggerEmotion("neutral");
                ESP_LOGI(TAG, "Conversation ended; restoring neutral emotion");
            }
        }
        
        // Sitting idle: 6s of silence -> bored, 20s -> suspicious (plus random looks/blinks)
        if (current_state == kDeviceStateIdle) {
            if (last_state != kDeviceStateIdle) {
                idle_since = xTaskGetTickCount();
                idle_bored_done = false;
                idle_suspicious_done = false;
                idle_sleepy_done = false;
                idle_asleep_done = false;
            }
            uint32_t idle_ms = (uint32_t)((xTaskGetTickCount() - idle_since) * portTICK_PERIOD_MS);
            if (!idle_bored_done && idle_ms > 6000) {
                idle_bored_done = true;
                set_eyes_mood("bored");
                ESP_LOGI(TAG, "Idle 6s -> eyes=bored");
            }
            if (!idle_suspicious_done && idle_ms > 20000) {
                idle_suspicious_done = true;
                set_eyes_mood("suspicious");
                ESP_LOGI(TAG, "Idle 20s -> eyes=suspicious");
            }
            if (!idle_sleepy_done && idle_ms > 120000) {
                idle_sleepy_done = true;
                set_eyes_mood("sleepy");
                ESP_LOGI(TAG, "Idle 120s -> eyes=sleepy (drowsy)");
            }
            if (!idle_asleep_done && idle_ms > 180000) {
                idle_asleep_done = true;
                // Panel sleep keeps the OLED RAM but stops driving the glass: the
                // screen goes fully dark and sips power. The eyes engine keeps running
                // so that the moment the boss returns (wake word / button) the face
                // is back instantly after disp_on_off(true).
                if (board->panel_ != nullptr &&
                    esp_lcd_panel_disp_on_off(board->panel_, false) == ESP_OK) {
                    board->panel_asleep_ = true;
                    ESP_LOGI(TAG, "Idle 180s -> panel asleep (screen off)");
                } else {
                    ESP_LOGW(TAG, "Idle 180s -> wanted panel sleep but disp_on_off failed");
                }
            }
        }
        
        // Update the previous device state
        last_state = current_state;
        
        // Delay for 100 milliseconds
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Note: this is never reached because the loop above is infinite
    delete params;
    vTaskDelete(NULL);
}

// Declare a static function used to process the AI response
static void ProcessAIResponseTask(void* arg) {
    char* message = (char*)arg;
    if (message) {
        // Get the emotion response controller
        EmotionResponseController* emotion_controller = nullptr;
        
        // Fetch it directly from the EmojiBoard
        if (g_board_instance) {
            // ProcessAIResponseTask is a friend of EmojiBoard, so it can access private members directly
            emotion_controller = g_board_instance->emotion_controller_;
            ESP_LOGI("AIResponseTask", "Retrieved emotion controller from EmojiBoard");
        }
        
        if (emotion_controller) {
            ESP_LOGI("AIResponseTask", "Processing AI response: %s", message);
            emotion_controller->ProcessAIResponse(message);
        } else {
            ESP_LOGW("AIResponseTask", "Could not retrieve emotion controller; unable to process AI response");
        }
    }
    
    // Free the message memory
    if (message) {
        free(message);
    }
    
    // Delete the task
    vTaskDelete(NULL);
}

// Implements EmojiDisplay::SetEmotion
void EmojiDisplay::SetEmotion(const char* emotion) {
    ESP_LOGI(TAG, "Xiaozhi AI framework recognized emotion: %s", emotion);
    
    // Call the base class SetEmotion to keep the original behaviour
    OledDisplay::SetEmotion(emotion);
    
    // Prevent recursive calls
    if (processing_ai_response_) {
        return;
    }
    
    // Map the Xiaozhi AI framework emotion onto our own expression motions
    if (board_ && board_->emotion_controller_) {
        std::string emotion_str(emotion);
        
        // Map the Xiaozhi AI framework emotion onto our own expression motions
        std::string mapped_emotion;
        
        // Happy family of expressions
        if (emotion_str == "happy") {
            mapped_emotion = "happy";
        } else if (emotion_str == "laughing") {
            mapped_emotion = "laughing";
        } else if (emotion_str == "funny") {
            mapped_emotion = "funny";
        } 
        // Sad family of expressions
        else if (emotion_str == "sad") {
            mapped_emotion = "sad";
        } else if (emotion_str == "crying") {
            mapped_emotion = "cry";
        } 
        // Angry expression
        else if (emotion_str == "angry") {
            mapped_emotion = "anger";
        } 
        // Surprised family of expressions
        else if (emotion_str == "surprised") {
            mapped_emotion = "surprise";
        } else if (emotion_str == "shocked") {
            mapped_emotion = "shocked";
        } 
        // Confused and thinking expressions
        else if (emotion_str == "confused") {
            mapped_emotion = "confused";
        } else if (emotion_str == "thinking") {
            mapped_emotion = "thinking";
        } 
        // Embarrassed expression
        else if (emotion_str == "embarrassed") {
            mapped_emotion = "awkward";
        } 
        // Sleeping expression
        else if (emotion_str == "sleepy") {
            mapped_emotion = "sleep";
        } 
        // Winking expression
        else if (emotion_str == "winking") {
            mapped_emotion = "blink";
        } 
        // Cool expression
        else if (emotion_str == "cool") {
            mapped_emotion = "cool";
        } 
        // Confident expression
        else if (emotion_str == "confident") {
            mapped_emotion = "confident";
        } 
        // Relaxed expression
        else if (emotion_str == "relaxed") {
            mapped_emotion = "relaxed";
        } 
        // Loving and kissing expressions
        else if (emotion_str == "loving") {
            mapped_emotion = "loving";
        } else if (emotion_str == "kissy") {
            mapped_emotion = "kissy";
        } 
        // Delicious expression
        else if (emotion_str == "delicious") {
            mapped_emotion = "delicious";
        } 
        // Silly expression
        else if (emotion_str == "silly") {
            mapped_emotion = "silly";
        } 
        // Neutral expression
        else if (emotion_str == "neutral") {
            mapped_emotion = "neutral";
        } 
        // For any other emotion, fall back to the default neutral expression
        else {
            mapped_emotion = "neutral";
            ESP_LOGW(TAG, "Unrecognized emotion type: %s; using the default neutral expression", emotion_str.c_str());
        }
        
        ESP_LOGI(TAG, "Mapped to our expression action: %s", mapped_emotion.c_str());

        // Switch the OliEyes mood from the emotion (the animation/servo pipeline is untouched)
        // No lock: SetMoodForEmotion only writes state consumed by the render task.
        if (board_ && board_->eyes_) {
            board_->eyes_->SetMoodForEmotion(mapped_emotion.c_str());
        }
        
        // Run the expression motion in a separate task so the main thread is not blocked
        // Copy the expression string because it will be used inside the task
        char* emotion_copy = strdup(mapped_emotion.c_str());
        if (emotion_copy) {
            xTaskCreate([](void* arg) {
                char* emotion = (char*)arg;
                // Get the EmojiBoard instance
                if (g_board_instance && g_board_instance->emotion_controller_) {
                    // TriggerEmotion is a public method, so call it to fire the expression motion
                    g_board_instance->emotion_controller_->TriggerEmotion(emotion);
                }
                // Free the copied string
                free(emotion);
                vTaskDelete(NULL);
            }, "emotion_task", 4096, emotion_copy, 1, NULL);
        }
    }
}

DECLARE_BOARD(EmojiBoard);