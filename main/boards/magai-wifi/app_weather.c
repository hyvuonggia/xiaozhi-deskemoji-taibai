/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "app_weather.h"
#include "esp_log.h"

static const char *TAG = "app_weather";

/**
 * @brief Weather code
 * 
 * Stores the current weather code, used to look up the matching weather icon
 * Defaults to the sunny weather code "100"
 */
char weather_icon[10] = "100";