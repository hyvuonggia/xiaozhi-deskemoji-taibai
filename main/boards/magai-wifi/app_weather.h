/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Weather code
 * 
 * Stores the current weather code, used to look up the corresponding weather icon
 */
extern char weather_icon[10];

#ifdef __cplusplus
}
#endif