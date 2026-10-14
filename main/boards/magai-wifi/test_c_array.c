/**
 * @file test_c_array.c
 * @brief Test program for C-array weather icons
 * 
 * Tests the display of weather icons in C array format
 */

#include "weather_display_new.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "test_c_array";

/**
 * @brief C array test task
 */
void test_c_array_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting C-array weather icon test");
    
    // Wait for system initialization to complete
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Call the C array test function
    weather_icon_test_c_array();
    
    ESP_LOGI(TAG, "C-array weather icon test complete");
    
    // Delete the task
    vTaskDelete(NULL);
}

/**
 * @brief Start the C array test
 */
void start_c_array_test(void)
{
    // Create the test task
    xTaskCreate(
        test_c_array_task,
        "test_c_array",
        4096,
        NULL,
        5,
        NULL
    );
}