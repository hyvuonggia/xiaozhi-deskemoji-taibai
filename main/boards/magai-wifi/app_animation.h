/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Update the weather icon
 * 
 * Update the weather icon on the UI based on the weather code
 */
void updata_weather_icon(void);

/**
 * @brief Initialize weather icon display
 * 
 * Initialize the weather icon display system, loading PNG weather icons from SPIFFS
 * 
 * @return esp_err_t Initialization result
 */
esp_err_t app_animation_start(void);

#ifdef __cplusplus
}
#endif