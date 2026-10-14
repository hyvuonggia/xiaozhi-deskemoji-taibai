#include "magai_wifi.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "led/circular_strip.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "weather.h"
#include "weather_display_new.h"
#include "app_animation.h"
extern "C" {
#include "test_c_array.h"
}

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_timer.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>
#include <esp_lcd_nv303b.h>


#define TAG "magai_wifi"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(lv_font_montserrat_48);

class NV303bDisplay : public SpiLcdDisplay {
private:
    lv_obj_t* weather_clock_container_ = nullptr;
    lv_obj_t* city_label_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* temp_label_ = nullptr;
    lv_obj_t* weather_icon_img_ = nullptr;   // Image icon object
    lv_obj_t* weather_text_label_ = nullptr;
    bool weather_clock_mode_ = false;
    
    void SetupWeatherClockUI() {
        DisplayLockGuard lock(this);
        
        // Create the weather clock container
        weather_clock_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(weather_clock_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(weather_clock_container_, current_theme_.background, 0);
        lv_obj_set_style_border_width(weather_clock_container_, 0, 0);
        lv_obj_add_flag(weather_clock_container_, LV_OBJ_FLAG_HIDDEN);
        
        // Create the city label (top)
        city_label_ = lv_label_create(weather_clock_container_);
        lv_obj_set_style_text_font(city_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(city_label_, current_theme_.text, 0);
        lv_label_set_text(city_label_, "Thành phố Vũ Hán");
        lv_obj_align(city_label_, LV_ALIGN_TOP_MID, 0, 10);
        
        // Create the time label (large text in the middle)
        time_label_ = lv_label_create(weather_clock_container_);
        lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(time_label_, current_theme_.text, 0);
        lv_label_set_text(time_label_, "11:04");
        lv_obj_align(time_label_, LV_ALIGN_CENTER, 0, -30);
        
        // Create the temperature label
        temp_label_ = lv_label_create(weather_clock_container_);
        lv_obj_set_style_text_font(temp_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(temp_label_, current_theme_.text, 0);
        lv_label_set_text(temp_label_, "25°C");
        lv_obj_align(temp_label_, LV_ALIGN_RIGHT_MID, -20, 0);  // right-center position
        
        // Initialize the weather icon using the new weather icon display module
        esp_err_t ret = weather_icon_new_init(weather_clock_container_);
        if (ret == ESP_OK) {
            weather_icon_img_ = weather_icon_new_get_obj();
            if (weather_icon_img_) {
                // Set a fixed size and position - adjusted to the left-center of the screen
                lv_obj_set_size(weather_icon_img_, 60, 60);  // reduce the size to avoid overlap
                lv_obj_align(weather_icon_img_, LV_ALIGN_LEFT_MID, 20, 0);  // left-center position
                lv_img_set_zoom(weather_icon_img_, 255);  // set the zoom
                
                // Ensure the icon visibility settings
                lv_obj_clear_flag(weather_icon_img_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_img_opa(weather_icon_img_, LV_OPA_COVER, 0);
                lv_obj_set_style_radius(weather_icon_img_, 0, 0);
                lv_obj_set_style_border_width(weather_icon_img_, 0, 0);
                
                // Make the background transparent
                lv_obj_set_style_bg_opa(weather_icon_img_, LV_OPA_TRANSP, 0);
                
                ESP_LOGI("MAGAI_WIFI", "Weather icon positioned at LEFT_MID(20, 0) with size 60x60");
                
                // Get the actual position info
                lv_coord_t icon_x = lv_obj_get_x(weather_icon_img_);
                lv_coord_t icon_y = lv_obj_get_y(weather_icon_img_);
                lv_coord_t icon_w = lv_obj_get_width(weather_icon_img_);
                lv_coord_t icon_h = lv_obj_get_height(weather_icon_img_);
                ESP_LOGI("MAGAI_WIFI", "Weather icon actual position: (%ld, %ld), size: %ldx%ld", 
                         (long)icon_x, (long)icon_y, (long)icon_w, (long)icon_h);
            }
        } else {
            ESP_LOGE("MAGAI_WIFI", "Failed to initialize weather icon display");
        }
        
        // Create the weather text label
        weather_text_label_ = lv_label_create(weather_clock_container_);
        lv_obj_set_style_text_font(weather_text_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(weather_text_label_, current_theme_.text, 0);
        lv_label_set_text(weather_text_label_, "Nắng");
        lv_obj_align(weather_text_label_, LV_ALIGN_BOTTOM_MID, 0, -10);
    }
    
public:
    NV303bDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy, 
                    {
                        .text_font = &font_puhui_16_4,
                        .icon_font = &font_awesome_16_4,
                        .emoji_font = font_emoji_32_init(),
                    }) {
        SetupWeatherClockUI();
    }
    
    // Update the weather clock display
    void UpdateWeatherClock(const std::string& city, const std::string& time, 
                           float temperature, const std::string& weather_text,
                           const std::string& weather_code = "") {
        DisplayLockGuard lock(this);
        
        if (city_label_) {
            lv_label_set_text(city_label_, city.c_str());
        }
        
        if (time_label_) {
            lv_label_set_text(time_label_, time.c_str());
        }
        
        if (temp_label_) {
            char temp_str[16];
            snprintf(temp_str, sizeof(temp_str), "%.1f°C", temperature);
            lv_label_set_text(temp_label_, temp_str);
        }
        
        // Update the weather icon - using the new weather icon display module
        if (!weather_code.empty()) {
            // Update the icon using the weather code
            weather_icon_new_update(weather_code.c_str());
            
            // Add detailed debug info to check the weather icon state
            if (weather_icon_img_) {
                lv_coord_t icon_x = lv_obj_get_x(weather_icon_img_);
                lv_coord_t icon_y = lv_obj_get_y(weather_icon_img_);
                lv_coord_t icon_w = lv_obj_get_width(weather_icon_img_);
                lv_coord_t icon_h = lv_obj_get_height(weather_icon_img_);
                bool is_hidden = lv_obj_has_flag(weather_icon_img_, LV_OBJ_FLAG_HIDDEN);
                lv_opa_t opa = lv_obj_get_style_opa(weather_icon_img_, 0);
                ESP_LOGI("MAGAI_WIFI", "Weather icon status: pos=(%ld,%ld), size=%ldx%ld, hidden=%s, opa=%d", 
                         (long)icon_x, (long)icon_y, (long)icon_w, (long)icon_h, 
                         is_hidden ? "true" : "false", opa);
                
                // Force the icon visible and move it to the foreground
                lv_obj_clear_flag(weather_icon_img_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(weather_icon_img_);
                lv_obj_invalidate(weather_icon_img_);
                
                // Make the background transparent
                lv_obj_set_style_bg_opa(weather_icon_img_, LV_OPA_TRANSP, 0);
            }
            
            // Add debug info to check the positions of other UI elements
            if (temp_label_) {
                lv_coord_t temp_x = lv_obj_get_x(temp_label_);
                lv_coord_t temp_y = lv_obj_get_y(temp_label_);
                lv_coord_t temp_w = lv_obj_get_width(temp_label_);
                lv_coord_t temp_h = lv_obj_get_height(temp_label_);
                ESP_LOGI("MAGAI_WIFI", "Temp label position: (%ld, %ld), size: %ldx%ld", 
                         (long)temp_x, (long)temp_y, (long)temp_w, (long)temp_h);
            }
            if (weather_text_label_) {
                lv_coord_t text_x = lv_obj_get_x(weather_text_label_);
                lv_coord_t text_y = lv_obj_get_y(weather_text_label_);
                ESP_LOGI("MAGAI_WIFI", "Weather text position: (%ld, %ld)", (long)text_x, (long)text_y);
            }
        }
        
        if (weather_text_label_) {
            lv_label_set_text(weather_text_label_, weather_text.c_str());
        }
    };
    
    // Switch to weather clock mode
    void ShowWeatherClock(bool show) {
        DisplayLockGuard lock(this);
        weather_clock_mode_ = show;
        
        ESP_LOGI("MAGAI_WIFI", "ShowWeatherClock: %s, container: %p", show ? "true" : "false", weather_clock_container_);
        
        if (show) {
            // Hide the regular UI elements
            if (container_) {
                lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
            }
            if (status_bar_) {
                lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
            }
            
            // Show the weather clock UI
            if (weather_clock_container_) {
                lv_obj_clear_flag(weather_clock_container_, LV_OBJ_FLAG_HIDDEN);
                ESP_LOGI("MAGAI_WIFI", "Weather clock container is now visible");
            }
        } else {
            // Hide the weather clock UI
            if (weather_clock_container_) {
                lv_obj_add_flag(weather_clock_container_, LV_OBJ_FLAG_HIDDEN);
                ESP_LOGI("MAGAI_WIFI", "Weather clock container is now hidden");
            }
            
            // Show the regular UI elements
            if (container_) {
                lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
            }
            if (status_bar_) {
                lv_obj_clear_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    
    // Check whether weather clock mode is active
    bool IsWeatherClockMode() const {
        return weather_clock_mode_;
    }
};

class magai_wifi : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus;
    esp_lcd_i80_bus_handle_t display_i80_bus_;
    NV303bDisplay* display_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    iot::Weather* weather_ = nullptr;
    bool idle_mode_ = false;
    // The following variables are no longer used
    // int idle_counter_ = 0;
    // const int kIdleTimeoutSeconds = 30; // enter weather clock mode after 30s of inactivity
    esp_timer_handle_t weather_clock_timer_ = nullptr; // timer handle, used to auto-update the time

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_config = {};
        i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_config.glitch_ignore_cnt = 7;
        i2c_bus_config.i2c_port = 0;
        i2c_bus_config.sda_io_num = DISPLAY_SDA_PIN;
        i2c_bus_config.scl_io_num = DISPLAY_SCL_PIN;
        i2c_bus_config.flags.enable_internal_pullup = true;
    
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus));
    
        i2c_device_config_t i2c_dev_conf = {};
        i2c_master_dev_handle_t i2c_dev_handle = NULL;
        i2c_dev_conf.dev_addr_length = I2C_ADDR_BIT_LEN_7;  // 7-bit address
        i2c_dev_conf.scl_speed_hz = 100000;
        i2c_dev_conf.device_address = 0X15;
        i2c_master_bus_add_device(i2c_bus, &i2c_dev_conf, &i2c_dev_handle);
    }

    void InitializeButtons() {
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }
    void InitializeNv303bDisplay() {
        esp_lcd_panel_io_handle_t pannel_io = nullptr;
        esp_lcd_panel_handle_t panel_handle = nullptr;

        esp_lcd_i80_bus_config_t bus_config = {};
        bus_config.clk_src = LCD_CLK_SRC_DEFAULT;
        bus_config.dc_gpio_num = DISPLAY_PIN_NUM_DC;
        bus_config.wr_gpio_num = DISPLAY_PIN_NUM_PCLK;
        bus_config.data_gpio_nums[0] = DISPLAY_PIN_NUM_DATA0;
        bus_config.data_gpio_nums[1] = DISPLAY_PIN_NUM_DATA1;
        bus_config.data_gpio_nums[2] = DISPLAY_PIN_NUM_DATA2;
        bus_config.data_gpio_nums[3] = DISPLAY_PIN_NUM_DATA3;
        bus_config.data_gpio_nums[4] = DISPLAY_PIN_NUM_DATA4;
        bus_config.data_gpio_nums[5] = DISPLAY_PIN_NUM_DATA5;
        bus_config.data_gpio_nums[6] = DISPLAY_PIN_NUM_DATA6;
        bus_config.data_gpio_nums[7] = DISPLAY_PIN_NUM_DATA7;
        bus_config.bus_width = 8;
        bus_config.max_transfer_bytes = DISPLAY_WIDTH * 100 * sizeof(uint16_t);
        bus_config.dma_burst_size = DISPLAY_DMA_BURST_SIZE;
        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &display_i80_bus_));

        esp_lcd_panel_io_i80_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_PIN_NUM_CS;
        io_config.pclk_hz = DISPLAY_LCD_PIXEL_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.dc_levels.dc_idle_level  = 0;
        io_config.dc_levels.dc_cmd_level   = 0;
        io_config.dc_levels.dc_dummy_level = 0;
        io_config.dc_levels.dc_data_level  = 1;
        io_config.flags.swap_color_bytes = 0;
        io_config.lcd_cmd_bits = DISPLAY_LCD_CMD_BITS;
        io_config.lcd_param_bits = DISPLAY_LCD_PARAM_BITS;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(display_i80_bus_, &io_config, &pannel_io));

        ESP_LOGI(TAG, "Install LCD driver of st7789");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_PIN_NUM_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv303b(pannel_io, &panel_config, &panel_handle));

        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);

        esp_lcd_panel_invert_color(panel_handle, true);
        esp_lcd_panel_swap_xy(panel_handle, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel_handle, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_set_gap(panel_handle, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        display_ = new NV303bDisplay(pannel_io, panel_handle,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

    }
    // IoT initialization: add devices visible to the AI
    void MountWeatherSpiffs() {
        // Mount the weather partition to /spiffs
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = "weather",
            .max_files = 5,
            .format_if_mount_failed = false,  // Don't format, we have pre-created images
        };
        esp_err_t ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount weather SPIFFS partition: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Weather SPIFFS partition mounted successfully at /spiffs");
            
            // Log SPIFFS info
            size_t total = 0, used = 0;
            ret = esp_spiffs_info("weather", &total, &used);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
            }
        }
    }
    
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        
        // Initialize the weather service
        weather_ = new iot::Weather();
        weather_->SetUserData(this); // set user data to this object for callbacks
        thing_manager.AddThing(weather_);
    }
    
    // Initialize the weather clock timer
    void InitializeWeatherClockTimer() {
        const esp_timer_create_args_t weather_clock_timer_args = {
            .callback = [](void* arg) {
                magai_wifi* board = static_cast<magai_wifi*>(arg);
                if (board && board->idle_mode_) {
                    board->UpdateWeatherClock();
                }
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "weather_clock_timer",
            .skip_unhandled_events = true
        };
        
        esp_timer_create(&weather_clock_timer_args, &weather_clock_timer_);
        // Update the time display once per minute
        esp_timer_start_periodic(weather_clock_timer_, 60 * 1000000); // 60 s * 1000000 microseconds
    }
    
    // Check the device state and update the weather clock display
    void CheckDeviceStateAndUpdateWeatherClock() {
        DeviceState current_state = Application::GetInstance().GetDeviceState();
        
        // Add debug info
        ESP_LOGI("MAGAI_WIFI", "Device state: %d, idle_mode: %s", current_state, idle_mode_ ? "true" : "false");
        
        // If the device is idle, show the weather clock
        if (current_state == kDeviceStateIdle && !idle_mode_) {
            idle_mode_ = true;
            ESP_LOGI("MAGAI_WIFI", "Entering weather clock mode");
            display_->ShowWeatherClock(true);
             UpdateWeatherClock(); // update the weather clock display immediately
        } 
        // If the device is not idle, hide the weather clock
        else if (current_state != kDeviceStateIdle && idle_mode_) {
            idle_mode_ = false;
            ESP_LOGI("MAGAI_WIFI", "Exiting weather clock mode");
            display_->ShowWeatherClock(false);
        }
    }
    
    // Update the weather clock display
    void UpdateWeatherClock() {
        // First check the device state
        CheckDeviceStateAndUpdateWeatherClock();
        
        // If weather clock mode is active, update the display
        if (idle_mode_) {
            // Check whether the weather data is ready
            bool weather_ready = weather_ && weather_->IsDataReady();
            ESP_LOGI(TAG, "Weather clock mode check: weather_=%p, IsDataReady=%s", weather_, weather_ready ? "true" : "false");
            if (weather_ready) {
            // Get the current time
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            
            // Format the time string
            std::stringstream time_ss;
            time_ss << std::setfill('0') << std::setw(2) << timeinfo.tm_hour << ":" 
                    << std::setfill('0') << std::setw(2) << timeinfo.tm_min;
            
            // Get the weather data
            // Since properties_ cannot be accessed directly, get the state through the Thing class's GetStateJson method
            cJSON *state_json = cJSON_Parse(weather_->GetStateJson().c_str());
            std::string city = "未知";
            float temperature = 0.0f;
            std::string weather_text = "未知";
            
            if (state_json) {
                // Get the state object from state_json
                cJSON *state = cJSON_GetObjectItem(state_json, "state");
                if (state) {
                    cJSON *city_json = cJSON_GetObjectItem(state, "city");
                    if (city_json && cJSON_IsString(city_json)) {
                        city = city_json->valuestring;
                    }
                    
                    cJSON *temp_json = cJSON_GetObjectItem(state, "temperature");
                    if (temp_json && cJSON_IsNumber(temp_json)) {
                        temperature = temp_json->valuedouble;
                    }
                    
                    cJSON *weather_json = cJSON_GetObjectItem(state, "weather");
                    if (weather_json && cJSON_IsString(weather_json)) {
                        weather_text = weather_json->valuestring;
                    }
                }
                
                cJSON_Delete(state_json);
            }
            
            // Get the weather code
            std::string weather_code = "";
            
            if (weather_) {
                weather_code = weather_->GetWeatherCode();
                
                // If the weather data is valid, actively update it once
                if (city == "未知" || weather_text == "未知" || temperature == 0.0f) {
                    ESP_LOGI(TAG, "Weather data is invalid; attempting to refresh it");
                    // Create a task to update the weather so the main thread is not blocked
                    xTaskCreate(
                        [](void* arg) {
                            iot::Weather* w = static_cast<iot::Weather*>(arg);
                            // First try to get the city automatically from the public IP
                            if (w->AutoDetectCity()) {
                                ESP_LOGI(TAG, "Automatically detected city via public IP: %s", w->GetCity().c_str());
                            } else {
                                ESP_LOGI(TAG, "Could not detect city via public IP; using the default city");
                            }
                            // Update the weather data
                            w->UpdateWeather();
                            vTaskDelete(NULL);
                        },
                        "weather_update_task",   // task name
                        8192,                    // stack size
                        weather_,                // task parameter
                        5,                       // task priority
                        NULL                     // task handle
                    );
                }
            }
            
            // Update the weather clock display
            // Translate known Chinese API descriptions only on the display path; retain the API value elsewhere.
            static const std::pair<const char*, const char*> weather_labels[] = {
                {"未知", "Không rõ"}, {"晴", "Nắng"}, {"多云", "Nhiều mây"}, {"少云", "Ít mây"},
                {"晴间多云", "Nắng, đôi lúc nhiều mây"}, {"阴", "Âm u"}, {"阵雨", "Mưa rào"},
                {"雷阵雨", "Mưa dông"}, {"小雨", "Mưa nhỏ"}, {"中雨", "Mưa vừa"},
                {"大雨", "Mưa to"}, {"暴雨", "Mưa rất to"}, {"小雪", "Tuyết nhẹ"},
                {"中雪", "Tuyết vừa"}, {"大雪", "Tuyết dày"}, {"雨夹雪", "Mưa tuyết"},
                {"雾", "Sương mù"}, {"霾", "Sương khói"}
            };
            std::string display_weather = weather_text;
            for (const auto& label : weather_labels) {
                if (weather_text == label.first) {
                    display_weather = label.second;
                    break;
                }
            }
            display_->UpdateWeatherClock(city, time_ss.str(), temperature, display_weather, weather_code);
            
            // Log for easier debugging
            ESP_LOGI(TAG, "Weather clock UI updated: city=%s, time=%s, temperature=%.1f°C, weather=%s, weather code=%s",
                     city.c_str(), time_ss.str().c_str(), temperature, weather_text.c_str(), weather_code.c_str());
            }
            else {
                // Get the current time
                time_t now;
                struct tm timeinfo;
                time(&now);
                localtime_r(&now, &timeinfo);
                
                // Format the time string
                std::stringstream time_ss;
                time_ss << std::setfill('0') << std::setw(2) << timeinfo.tm_hour << ":" 
                        << std::setfill('0') << std::setw(2) << timeinfo.tm_min;
                
                // Show the loading hint
                display_->UpdateWeatherClock("Đang tải...", time_ss.str(), 0.0f, "Đang lấy dữ liệu thời tiết", "");
                ESP_LOGI(TAG, "Weather data is not ready; showing the loading message");
            }
        }
    }
    
    // Check for user activity (button presses, etc.) - this method is no longer used because we now use a timer to periodically check the device state
    /*
    bool HasUserActivity() {
        // Check for user activity, which can be inferred from the Application class's device state
        // Or check the button state
        DeviceState current_state = Application::GetInstance().GetDeviceState();
        bool is_active = (current_state != kDeviceStateIdle); // if not idle, consider there to be activity
        
        // Since the Button class has no IsPressed method, only the device state is used for now
        return is_active;
        // If the button state must be checked, an IsPressed method needs to be added to the Button class
        // Or check the button state in another way
    }
    */

public:
    magai_wifi() :
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
            InitializeButtons();
            InitializeIot();
            InitializeI2c();
            InitializeNv303bDisplay();
            GetBacklight()->RestoreBrightness();
            
            // Mount the weather icon SPIFFS partition
            MountWeatherSpiffs();
            
            // The PNG decoder test was moved to Application::Start and runs after the xiaozhi AI framework is fully initialized
            
            // Initialize the weather icon MMAP filesystem
            esp_err_t ret = app_animation_start();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize weather animation system: %s", esp_err_to_name(ret));
            }
            
            // Initial state check: show the weather clock if idle
            CheckDeviceStateAndUpdateWeatherClock();
            
            // Periodically update the weather clock UI with a timer
            InitializeWeatherClockTimer();
            
            // Start the C-array format weather icon test
            start_c_array_test();
            ESP_LOGI(TAG, "C-array weather icon test started");
    }
    
    ~magai_wifi() {
        // Clean up timer resources
        if (weather_clock_timer_ != nullptr) {
            esp_timer_stop(weather_clock_timer_);
            esp_timer_delete(weather_clock_timer_);
            weather_clock_timer_ = nullptr;
        }
        
        delete weather_;
    }

    // Custom Led class that overrides OnStateChanged to update the weather clock UI when the device state changes
    class MagaiLed : public CircularStrip {
    private:
        magai_wifi* board_;
        
    public:
        MagaiLed(int gpio_num, int led_num, magai_wifi* board) 
            : CircularStrip(static_cast<gpio_num_t>(gpio_num), led_num), board_(board) {}
        
        virtual void OnStateChanged() override {
            // First call the parent class's OnStateChanged method
            CircularStrip::OnStateChanged();
            
            // Check the device state and update the weather clock display
            board_->CheckDeviceStateAndUpdateWeatherClock();
            
            // If idle, update the weather clock display once
            if (board_->idle_mode_) {
                // Get the current device state
                DeviceState current_state = Application::GetInstance().GetDeviceState();
                
                // If the device state is idle, trigger a weather data update
                if (current_state == kDeviceStateIdle && board_->weather_ != nullptr) {
                    // Create a task to update the weather so the main thread is not blocked
                    xTaskCreate(
                        [](void* arg) {
                            iot::Weather* w = static_cast<iot::Weather*>(arg);
                            // First try to get the city automatically from the public IP
                            if (w->AutoDetectCity()) {
                                ESP_LOGI(TAG, "Automatically detected city via public IP: %s", w->GetCity().c_str());
                            } else {
                                ESP_LOGI(TAG, "Could not detect city via public IP; using the default city");
                            }
                            // Update the weather data
                            w->UpdateWeather();
                            
                            // Add a delay to ensure the data is ready
                            vTaskDelay(pdMS_TO_TICKS(200));
                            
                            // Notify the main thread to update the UI
                            magai_wifi* board = static_cast<magai_wifi*>(w->GetUserData());
                            if (board && w->IsDataReady()) {
                                board->UpdateWeatherClock();
                                ESP_LOGI(TAG, "Weather data is ready; triggering UI update");
                            }
                            
                            vTaskDelete(NULL);
                        },
                        "weather_update_init",   // task name
                        8192,                    // stack size
                        board_->weather_,        // task parameter
                        5,                       // task priority
                        NULL                     // task handle
                    );
                    ESP_LOGI(TAG, "Triggered weather data update after device entered idle state");
                }
                
                // No longer call UpdateWeatherClock directly; the task triggers the update once the weather data is ready
                // If the weather data is ready, the UI can be updated immediately
                if (board_->weather_ && board_->weather_->IsDataReady()) {
                    board_->UpdateWeatherClock();
                }
            }
        }
    };
    
    virtual Led* GetLed() override {
        static MagaiLed led(BUILTIN_LED_GPIO, BUILTIN_LED_NUM, this);
        return &led;
    }
    
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(magai_wifi);
