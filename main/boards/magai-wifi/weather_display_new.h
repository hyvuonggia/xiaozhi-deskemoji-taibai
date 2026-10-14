/**
 * @file weather_display_new.h
 * @brief Weather icon display module header file - new version
 * 
 * This module provides weather icon display related functionality, including:
 * 1. Initialize the weather icon display
 * 2. Update the weather icon
 * 3. Display a weather icon of the given type
 * 4. Get the weather icon object
 * 
 * This module loads PNG weather icons directly, accessed through the SPIFFS filesystem.
 */

#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the weather icon display module
 * 
 * Initialize weather icon display related resources.
 * Must be called before using any other weather icon function.
 * 
 * @param parent Parent object; the weather icon is created as a child of this object
 * @return esp_err_t Initialization result
 *         - ESP_OK: initialization succeeded
 *         - ESP_FAIL: initialization failed
 */
esp_err_t weather_icon_new_init(lv_obj_t *parent);

/**
 * @brief Display the weather icon for the given weather code
 * 
 * Display the weather icon corresponding to the provided weather code.
 * Load the corresponding PNG icon file from the SPIFFS filesystem.
 * 
 * @param weather_code Weather code string (e.g. "100", "101", "200")
 */
void weather_icon_new_show(const char *weather_code);

/**
 * @brief Update the weather icon
 * 
 * Update the weather icon according to the provided weather code.
 * Use the weather code directly as the file name and load the corresponding PNG icon from the SPIFFS filesystem.
 * 
 * @param weather_code Weather code string returned by the Seniverse weather API
 */
void weather_icon_new_update(const char *weather_code);

/**
 * @brief Display a weather icon of the given type
 * 
 * Display the weather icon for the provided weather type index directly.
 * Load the corresponding PNG icon from the SPIFFS filesystem.
 * 
 * @param type Weather type index (0-6):
 *             0-sunny, 1-cloudy, 2-overcast, 3-rainy, 4-snowy, 5-foggy, 6-thunderstorm
 */
void weather_icon_new_show_type(int type);

/**
 * @brief Test the SPIFFS partition and the weather icon files
 * 
 * Test function used to diagnose weather icon display problems.
 * Checks the SPIFFS mount status, the file listing and file reads.
 */
void weather_icon_test_spiffs(void);

/**
 * @brief Test the PNG decoder
 * 
 * Test function that checks whether the PNG decoder works correctly.
 * Checks PNG decoder initialization and decoding.
 */
void weather_icon_test_png_decoder(void);

/**
 * @brief Deinitialize the weather icon display module
 * 
 * Release weather icon display related resources.
 * Call this function when the weather icon feature is no longer needed.
 */
void weather_icon_new_deinit(void);

/**
 * @brief Get the weather icon object
 * 
 * Returns the current weather icon object for further customization or manipulation.
 * 
 * @return lv_obj_t* Weather icon object, or NULL if not initialized
 */
lv_obj_t* weather_icon_new_get_obj(void);

/**
 * @brief Test displaying a weather icon in C file format
 * 
 * Test function that verifies whether weather icons in C array format display correctly.
 * Uses the image data from the converted C file directly for a display test.
 */
void weather_icon_test_c_array(void);

#ifdef __cplusplus
}
#endif