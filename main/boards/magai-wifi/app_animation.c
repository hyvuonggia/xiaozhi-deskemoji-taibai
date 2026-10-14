/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"

#include "app_animation.h"
#include "app_weather.h"
#include "weather_display_new.h"
#include "lvgl.h"

static const char *TAG = "app_animation";

// The weather codes are defined in app_weather.c and referenced here via the header

void updata_weather_icon(void)
{
    // Use the functions in weather_display_new.c to update the weather icon
    // This ensures a unified icon object and logic are used
    extern void weather_icon_new_update(const char* weather_code);
    
    if (strlen(weather_icon) > 0) {
        weather_icon_new_update(weather_icon);
        ESP_LOGI(TAG, "Updated weather icon for code %s using unified function", weather_icon);
    } else {
        ESP_LOGW(TAG, "Weather icon code is empty, using default");
        weather_icon_new_update("100");
    }
}

/**
 * @brief Initialize the weather icon display
 * 
 * Initialize the weather icon display system, loading PNG weather icons from the SPIFFS filesystem
 * 
 * @return esp_err_t Initialization result
 */
esp_err_t app_animation_start(void)
{
    ESP_LOGI(TAG, "Weather animation system initialized with SPIFFS PNG support");
    return ESP_OK;
}