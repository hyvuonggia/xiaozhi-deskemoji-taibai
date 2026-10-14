/**
 * @file weather_display_new.c
 * @brief weather icon display module implementation - new version
 * 
 * This module implements weather-icon display functionality, loading PNG weather icon files from the SPIFFS filesystem.
 */

#include "weather_display_new.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <esp_spiffs.h>
#include "weather_icons_c/weather_icons.h"

#define TAG "weather_display_new"

// Global variables
static lv_obj_t *weather_icon_obj = NULL;
static char weather_icon_path[64];         // PNG icon file path

/**
 * @brief Initialize the weather icon display module
 * 
 * Initializes the resources for weather icon display.
 * This function must be called before using any other weather icon feature.
 * 
 * @param parent Parent object; the weather icon is created as a child of it
 * @return esp_err_t Initialization result
 *         - ESP_OK: initialization succeeded
 *         - ESP_FAIL: initialization failed
 */
esp_err_t weather_icon_new_init(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Initializing weather icon display module");
    
    // Create the weather icon object - using an image object
    weather_icon_obj = lv_image_create(parent);
    if (weather_icon_obj == NULL) {
        ESP_LOGE(TAG, "Failed to create weather icon object");
        return ESP_FAIL;
    }
    
    // Set the icon position and size
    lv_obj_set_size(weather_icon_obj, 110, 110);  // Set an appropriate size
    lv_obj_align(weather_icon_obj, LV_ALIGN_CENTER, 0, 0);  // Center alignment
    
    // Make the object visible
    lv_obj_clear_flag(weather_icon_obj, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "Weather icon display module initialized successfully");
    return ESP_OK;
}

/**
 * @brief Display the weather icon for a given weather code
 * 
 * Displays the matching weather icon for the given weather code.
 * Loads the corresponding PNG icon file from the SPIFFS filesystem.
 * 
 * @param weather_code Weather code string (e.g. "100", "101", "200")
 */
void weather_icon_new_show(const char *weather_code)
{
    // Check whether the icon object has been initialized
    if (weather_icon_obj == NULL) {
        ESP_LOGE(TAG, "Weather icon object not initialized");
        return;
    }
    
    // Check whether the weather code is valid
    if (weather_code == NULL || strlen(weather_code) == 0) {
        ESP_LOGW(TAG, "Invalid weather code, using default sunny icon");
        weather_code = "100"; // Default to the sunny code
    }
    
    ESP_LOGI(TAG, "Displaying weather icon for code: %s", weather_code);
    
    // Build the PNG icon file path (using the LVGL drive prefix - drive letter A corresponds to ASCII 65)
    snprintf(weather_icon_path, sizeof(weather_icon_path), "A:/spiffs/%s.png", weather_code);
    
    // Check whether the file exists (using POSIX path format)
    char posix_path[128];
    snprintf(posix_path, sizeof(posix_path), "/spiffs/%s.png", weather_code);
    FILE *file = fopen(posix_path, "r");
    if (file != NULL) {
        fclose(file);
        ESP_LOGI(TAG, "Found weather icon: %s", weather_icon_path);
        
        // Set the PNG image source
        lv_image_set_src(weather_icon_obj, weather_icon_path);
        
        // Ensure the icon is visible
        lv_obj_clear_flag(weather_icon_obj, LV_OBJ_FLAG_HIDDEN);
        // Move to the foreground
        lv_obj_move_foreground(weather_icon_obj);
        // Force a display refresh
        lv_obj_invalidate(weather_icon_obj);
        
        ESP_LOGI(TAG, "Weather icon loaded successfully for code: %s", weather_code);
    } else {
        ESP_LOGW(TAG, "Weather icon file not found: %s, trying fallback", weather_icon_path);
        
        // Try to use the default icon
        snprintf(weather_icon_path, sizeof(weather_icon_path), "A:/spiffs/100.png");
        snprintf(posix_path, sizeof(posix_path), "/spiffs/100.png");
        file = fopen(posix_path, "r");
        if (file != NULL) {
            fclose(file);
            lv_image_set_src(weather_icon_obj, weather_icon_path);
            ESP_LOGI(TAG, "Using fallback sunny icon");
        } else {
            ESP_LOGE(TAG, "No weather icons available in SPIFFS");
        }
    }
}

/**
 * @brief Update the weather icon
 * 
 * Updates the weather icon for the given weather code.
 * Uses the weather code directly as the file name and loads the corresponding PNG icon from the SPIFFS filesystem.
 * 
 * @param weather_code Weather code string returned by the Seniverse weather API
 */
void weather_icon_new_update(const char *weather_code)
{
    // Check whether the icon object has been initialized
    if (weather_icon_obj == NULL) {
        ESP_LOGE(TAG, "Weather icon object not initialized");
        return;
    }
    
    // Check whether the weather code is valid
    if (weather_code == NULL || strlen(weather_code) == 0) {
        ESP_LOGW(TAG, "Invalid weather code, using default sunny icon");
        weather_code = "100"; // Default to the sunny code
    }
    
    ESP_LOGI(TAG, "Updating weather icon for code: %s", weather_code);
    
    // Weather code mapping: map the short codes returned by the API to standard three-digit codes
    char mapped_code[10];
    if (strcmp(weather_code, "0") == 0 || strcmp(weather_code, "1") == 0) {
        strcpy(mapped_code, "100"); // sunny
    } else if (strcmp(weather_code, "2") == 0) {
        strcpy(mapped_code, "101"); // cloudy
    } else if (strcmp(weather_code, "3") == 0) {
        strcpy(mapped_code, "104"); // overcast
    } else if (strlen(weather_code) < 3) {
        // For other short codes, try zero-padding to three digits
        snprintf(mapped_code, sizeof(mapped_code), "%03d", atoi(weather_code));
    } else {
        // Already a three-digit code, use it as is
        strncpy(mapped_code, weather_code, sizeof(mapped_code) - 1);
        mapped_code[sizeof(mapped_code) - 1] = '\0';
    }
    
    ESP_LOGI(TAG, "Mapped weather code from %s to %s", weather_code, mapped_code);
    
    // Build the PNG icon file path (using the LVGL drive prefix - drive letter A corresponds to ASCII 65)
        snprintf(weather_icon_path, sizeof(weather_icon_path), "A:/spiffs/%s.png", mapped_code);
    
    // Check whether the file exists (using POSIX path format)
    char posix_path[128];
    snprintf(posix_path, sizeof(posix_path), "/spiffs/%s.png", mapped_code);
    FILE *file = fopen(posix_path, "r");
    if (file != NULL) {
        fclose(file);
        ESP_LOGI(TAG, "Found weather icon: %s", weather_icon_path);
        
        // Set the PNG image source
        ESP_LOGI(TAG, "Setting image source: %s", weather_icon_path);
        lv_image_set_src(weather_icon_obj, weather_icon_path);
        
        // Process LVGL tasks to ensure the image loads
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait 50 ms for the image to load
        
        // Check whether the image loaded successfully
        const void* src = lv_image_get_src(weather_icon_obj);
        lv_coord_t w = lv_obj_get_width(weather_icon_obj);
        lv_coord_t h = lv_obj_get_height(weather_icon_obj);
        ESP_LOGI(TAG, "Image source: %p, size: %dx%d", src, (int)w, (int)h);
        
        // Apply image optimization settings
        // Auto-size setting commented out to keep a fixed size
        // lv_obj_set_size(weather_icon_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // Set to content-adaptive size
        lv_obj_set_style_img_recolor_opa(weather_icon_obj, LV_OPA_0, 0);  // Disable recoloring
        lv_obj_set_style_transform_angle(weather_icon_obj, 0, 0);  // Ensure the angle is 0
        lv_obj_set_style_transform_zoom(weather_icon_obj, 256, 0);  // Set zoom to 1.0x
        
        // Ensure the icon is visible
        lv_obj_clear_flag(weather_icon_obj, LV_OBJ_FLAG_HIDDEN);
        // Move to the foreground
        lv_obj_move_foreground(weather_icon_obj);
        // Force a display refresh
        lv_obj_invalidate(weather_icon_obj);
        
        // Check the size again
        w = lv_obj_get_width(weather_icon_obj);
        h = lv_obj_get_height(weather_icon_obj);
        ESP_LOGI(TAG, "Final image size: %dx%d", (int)w, (int)h);
        
        ESP_LOGI(TAG, "Weather icon updated successfully for code: %s", weather_code);
    } else {
        ESP_LOGW(TAG, "Weather icon file not found: %s, trying fallback", weather_icon_path);
        
        // Try to use the default icon
        snprintf(weather_icon_path, sizeof(weather_icon_path), "A:/spiffs/100.png");
        snprintf(posix_path, sizeof(posix_path), "/spiffs/100.png");
        file = fopen(posix_path, "r");
        if (file != NULL) {
            fclose(file);
            lv_image_set_src(weather_icon_obj, weather_icon_path);
            ESP_LOGI(TAG, "Using fallback sunny icon");
        } else {
            ESP_LOGE(TAG, "No weather icons available in SPIFFS");
        }
    }
}

/**
 * @brief Display the weather icon for a given type
 * 
 * Directly displays the matching weather icon for the given weather type index.
 * Loads the corresponding PNG icon from the SPIFFS filesystem.
 * 
 * @param type Weather type index (0-6):
 *             0-sunny, 1-cloudy, 2-overcast, 3-rainy, 4-snowy, 5-foggy, 6-thunderstorm
 */
void weather_icon_new_show_type(int type)
{
    // Check whether the icon object has been initialized
    if (weather_icon_obj == NULL) {
        ESP_LOGE(TAG, "Weather icon object not initialized");
        return;
    }
    
    // Check whether the weather type is valid
    if (type < 0 || type > 6) {
        ESP_LOGW(TAG, "Invalid weather type: %d, using default sunny type", type);
        type = 0; // Default to the sunny type
    }
    
    // Weather codes corresponding to each weather type
    const char *weather_codes[] = {
        "100", // sunny
        "104", // cloudy
        "101", // cloudy (previously overcast)
        "300", // rainy
        "400", // snowy
        "500", // foggy
        "302"  // thunderstorm
    };
    
    // Build the PNG icon file path (using the LVGL drive prefix - drive letter A corresponds to ASCII 65)
    snprintf(weather_icon_path, sizeof(weather_icon_path), "A:/spiffs/%s.png", weather_codes[type]);
    
    // Check whether the file exists (using POSIX path format)
    char posix_path[128];
    snprintf(posix_path, sizeof(posix_path), "/spiffs/%s.png", weather_codes[type]);
    FILE *file = fopen(posix_path, "r");
    if (file != NULL) {
        fclose(file);
        ESP_LOGI(TAG, "Found weather icon: %s", weather_icon_path);
        
        // Set the PNG image source
        lv_image_set_src(weather_icon_obj, weather_icon_path);
        
        // Apply image optimization settings
        // Auto-size setting commented out to keep a fixed size
        // lv_obj_set_size(weather_icon_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // Set to content-adaptive size
        lv_obj_set_style_img_recolor_opa(weather_icon_obj, LV_OPA_0, 0);  // Disable recoloring
        lv_obj_set_style_transform_angle(weather_icon_obj, 0, 0);  // Ensure the angle is 0
        lv_obj_set_style_transform_zoom(weather_icon_obj, 256, 0);  // Set zoom to 1.0x
        
        // Ensure the icon is visible
        lv_obj_clear_flag(weather_icon_obj, LV_OBJ_FLAG_HIDDEN);
        // Move to the foreground
        lv_obj_move_foreground(weather_icon_obj);
        // Force a display refresh
        lv_obj_invalidate(weather_icon_obj);
        
        ESP_LOGI(TAG, "Loading weather icon from SPIFFS: %s with optimization", weather_icon_path);
    } else {
        ESP_LOGW(TAG, "Weather icon file not found: %s", weather_icon_path);
    }
}

/**
 * @brief Test the SPIFFS partition and weather icon files
 * 
 * Test function for diagnosing weather icon display problems.
 * Checks the SPIFFS mount status, file listing, and file reads.
 */
void weather_icon_test_spiffs(void)
{
    ESP_LOGI(TAG, "=== Starting SPIFFS weather icon diagnostic test ===");
    
    // 1. Check SPIFFS partition information
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info("weather", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve SPIFFS information: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPIFFS partition: total=%d KB, used=%d KB, available=%d KB",
             total/1024, used/1024, (total-used)/1024);
    
    // 2. List the files in the /spiffs directory
    ESP_LOGI(TAG, "Files in /spiffs:");
    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Could not open /spiffs directory");
        return;
    }
    
    struct dirent *entry;
    int file_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Only show files, not directories
            file_count++;
            ESP_LOGI(TAG, "  File %d: %s", file_count, entry->d_name);
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "Found %d files in total", file_count);
    
    // 3. Test whether the common weather icon files exist
    ESP_LOGI(TAG, "Checking common weather icon files:");
    const char* test_icons[] = {"100", "101", "104", "300", "400", "500", "302"};
    const char* icon_names[] = {"Sunny", "Cloudy", "Overcast", "Rainy", "Snowy", "Foggy", "Thunderstorm"};
    
    for (int i = 0; i < 7; i++) {
        char filepath[64];
        snprintf(filepath, sizeof(filepath), "/spiffs/%s.png", test_icons[i]);
        
        FILE *file = fopen(filepath, "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fclose(file);
            ESP_LOGI(TAG, "  ✓ %s (%s): %ld bytes", icon_names[i], filepath, size);
        } else {
            ESP_LOGE(TAG, "  ✗ %s (%s): file not found", icon_names[i], filepath);
        }
    }
    
    // 4. Validate the PNG header
    FILE *file = fopen("/spiffs/100.png", "rb");
    if (file) {
        uint8_t header[8];
        size_t read_size = fread(header, 1, 8, file);
        fclose(file);
        
        if (read_size == 8) {
            ESP_LOGI(TAG, "100.png header: %02X %02X %02X %02X %02X %02X %02X %02X",
                     header[0], header[1], header[2], header[3], 
                     header[4], header[5], header[6], header[7]);
            
            // The PNG header should be: 89 50 4E 47 0D 0A 1A 0A
            if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) {
                ESP_LOGI(TAG, "✓ PNG header validation passed");
            } else {
                ESP_LOGE(TAG, "✗ PNG header validation failed");
            }
        }
    }
    
    ESP_LOGI(TAG, "=== SPIFFS weather icon diagnostic test complete ===");
}

/**
 * @brief Test the PNG decoder
 * 
 * Test function that verifies the LVGL PNG decoder works correctly.
 * Creates a temporary image object to actually test PNG decoding and display.
 */
void weather_icon_test_png_decoder(void)
{
    ESP_LOGI(TAG, "=== Starting PNG decoder test ===");
    
    // Test whether LVGL can load a PNG image correctly
    const char* test_file_lvgl = "A:/spiffs/100.png";  // LVGL path format
    const char* test_file_posix = "/spiffs/100.png";   // POSIX path format
    
    // Check whether the file exists (using the POSIX path)
    FILE *file = fopen(test_file_posix, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Test file %s does not exist", test_file_posix);
        return;
    }
    fclose(file);
    ESP_LOGI(TAG, "File existence check passed: %s", test_file_posix);
    
    // Validate the PNG header
    file = fopen(test_file_posix, "rb");
    if (file) {
        uint8_t header[8];
        size_t read_size = fread(header, 1, 8, file);
        fclose(file);
        
        if (read_size == 8) {
            ESP_LOGI(TAG, "PNG header: %02X %02X %02X %02X %02X %02X %02X %02X",
                     header[0], header[1], header[2], header[3], 
                     header[4], header[5], header[6], header[7]);
            
            // The PNG header should be: 89 50 4E 47 0D 0A 1A 0A
            if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) {
                ESP_LOGI(TAG, "✓ PNG header validation passed; file format is valid");
            } else {
                ESP_LOGE(TAG, "✗ PNG header validation failed");
                return;
            }
        }
    }
    
    // Actually test the LVGL PNG decoder
    ESP_LOGI(TAG, "Starting LVGL PNG decoder test...");
    
    // Get the current active screen
    lv_obj_t* screen = lv_screen_active();
    if (!screen) {
        ESP_LOGE(TAG, "Could not get active screen");
        return;
    }
    
    // Create a temporary image object for testing
    lv_obj_t* test_img = lv_image_create(screen);
    if (!test_img) {
        ESP_LOGE(TAG, "Failed to create test image object");
        return;
    }
    
    ESP_LOGI(TAG, "Temporary image object created successfully");
    
    // Set the image position (center of the screen, a more visible spot)
    lv_obj_set_pos(test_img, (LV_HOR_RES - 60) / 2, (LV_VER_RES - 60) / 2);
    lv_obj_set_size(test_img, 60, 60);
    
    // Raise the layer level so it is drawn on top
    lv_obj_move_foreground(test_img);
    
    // Try to load the PNG image
    ESP_LOGI(TAG, "Attempting to load PNG image: %s", test_file_lvgl);
    lv_image_set_src(test_img, test_file_lvgl);
    
    // Ensure the image is visible and set its opacity
    lv_obj_clear_flag(test_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(test_img, LV_OPA_COVER, 0);
    lv_obj_move_foreground(test_img);
    
    // Process LVGL tasks to ensure the image is handled
    for (int i = 0; i < 5; i++) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Check whether the image loaded successfully
    const void* src = lv_image_get_src(test_img);
    if (src) {
        ESP_LOGI(TAG, "✓ PNG image source set successfully");
        
        // Get the actual image dimensions
         lv_coord_t width = lv_obj_get_width(test_img);
         lv_coord_t height = lv_obj_get_height(test_img);
         ESP_LOGI(TAG, "Image dimensions: %dx%d", (int)width, (int)height);
        
        // Show the test image for 3 seconds
        ESP_LOGI(TAG, "Test image will be displayed for 3 seconds; check for a weather icon in the center of the screen");
        vTaskDelay(pdMS_TO_TICKS(3000));
        
    } else {
        ESP_LOGE(TAG, "✗ Failed to load PNG image");
    }
    
    // Clean up the test object (delayed delete to avoid memory issues)
    ESP_LOGI(TAG, "Cleaning up test object...");
    lv_obj_delete_delayed(test_img, 100);  // Delete after a 100 ms delay
    
    // Process LVGL tasks to ensure the deletion completes
    for (int i = 0; i < 3; i++) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "=== PNG decoder test complete ===");
}

/**
 * @brief Deinitialize the weather icon display module
 * 
 * Cleans up the resources related to weather icon display.
 * Call this function when the weather icon feature is no longer needed.
 */
void weather_icon_new_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing weather icon display module");
    
    // Clear the path buffer
    memset(weather_icon_path, 0, sizeof(weather_icon_path));
    
    // Note: weather_icon_obj is managed by LVGL and does not need to be freed manually
    weather_icon_obj = NULL;
    
    ESP_LOGI(TAG, "Weather icon display module deinitialized");
}

/**
 * @brief Get the weather icon object
 * 
 * Returns the current weather icon object for further customization or manipulation.
 * 
 * @return lv_obj_t* The weather icon object, or NULL if not initialized
 */
lv_obj_t* weather_icon_new_get_obj(void)
{
    // Return the weather icon object pointer
    return weather_icon_obj;
}

/**
 * @brief Test weather icon display from C file format
 * 
 * Test function that verifies weather icons in C array format display correctly.
 * Uses the image data in the converted C file directly for the display test.
 */
void weather_icon_test_c_array(void)
{
    ESP_LOGI(TAG, "=== Starting C-array weather icon test ===");
    
    // Get the current active screen
    lv_obj_t* screen = lv_screen_active();
    if (!screen) {
        ESP_LOGE(TAG, "Could not get active screen");
        return;
    }
    
    // Create a temporary image object for testing
    lv_obj_t* test_img = lv_image_create(screen);
    if (!test_img) {
        ESP_LOGE(TAG, "Failed to create test image object");
        return;
    }
    
    ESP_LOGI(TAG, "Temporary image object created successfully");
    
    // Set the image position (center of the screen)
    lv_obj_set_pos(test_img, (LV_HOR_RES - 60) / 2, (LV_VER_RES - 60) / 2);
    lv_obj_set_size(test_img, 60, 60);
    
    // Raise the layer level so it is drawn on top
    lv_obj_move_foreground(test_img);
    
    // Try to load the C array image
    ESP_LOGI(TAG, "Attempting to load C-array image data");
    
    // Use the weather icon variable defined in the header file
    lv_image_set_src(test_img, &_icon_100);
    
    // Ensure the image is visible and set its opacity
    lv_obj_clear_flag(test_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(test_img, LV_OPA_COVER, 0);
    lv_obj_move_foreground(test_img);
    
    // Process LVGL tasks to ensure the image is handled
    for (int i = 0; i < 5; i++) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Check whether the image loaded successfully
    const void* src = lv_image_get_src(test_img);
    if (src) {
        ESP_LOGI(TAG, "✓ C-array image source set successfully");
        
        // Get the actual image dimensions
        lv_coord_t width = lv_obj_get_width(test_img);
        lv_coord_t height = lv_obj_get_height(test_img);
        ESP_LOGI(TAG, "Image dimensions: %dx%d", (int)width, (int)height);
        
        // Show the test image for 3 seconds
        ESP_LOGI(TAG, "Test image will be displayed for 3 seconds; check for a weather icon in the center of the screen");
        vTaskDelay(pdMS_TO_TICKS(3000));
        
    } else {
        ESP_LOGE(TAG, "✗ Failed to load C-array image");
    }
    
    // Clean up the test object (delayed delete to avoid memory issues)
    ESP_LOGI(TAG, "Cleaning up test object...");
    lv_obj_delete_delayed(test_img, 100);  // Delete after a 100 ms delay
    
    // Process LVGL tasks to ensure the deletion completes
    for (int i = 0; i < 3; i++) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "=== C-array weather icon test complete ===");
}