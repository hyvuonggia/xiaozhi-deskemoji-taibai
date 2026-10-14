#include "iot/thing.h"
#include "../common/board.h"
#include "../common/wifi_board.h"
#include "lvgl.h"
#include "weather.h"  // Declaration of the Weather class
#include "application.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "magai_wifi.h"  // Declaration of the magai_wifi class

// Weather icon implementation removed

#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <map>

static const char* TAG = "Weather";

// URL to obtain the public IP address
#define HTTPS_HuoQuIP           "https://ipinfo.io/json"
// Amap (Gaode) geolocation API URL
#define HTTPS_DingWei           "http://restapi.amap.com/v3/ip?key=e673da4f70707f787c7b00443211602b&ip="
// Seniverse (Xinzhi) weather API URL
#define HTTPS_XinZHi_Key        "S8r2hr2JxTta2E0W8"
#define HTTPS_XinZHi            "https://api.seniverse.com/v3/weather/now.json?key=" HTTPS_XinZHi_Key "&language=zh-Hans&unit=c&location="

// Current-weather data structure (only essential fields kept after optimization)
typedef struct {
    char text[20];                  // weather condition
    char code[10];                  // weather condition code
    char temperature[10];           // current temperature
    char city[30];                  // Chinese city name
} _Weather_Data;

namespace iot {

// Global variable caching the public IP
static std::string cached_ip;

// HTTP response callback
esp_err_t HttpEventHandler(esp_http_client_event_t *evt) {
    std::string* response_buffer = static_cast<std::string*>(evt->user_data);
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Append received data to the response buffer
            if (evt->data_len > 0) {
                response_buffer->append((char*)evt->data, evt->data_len);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// HTTP request function - with retry mechanism and error handling
int HTTP_Read_Data(const char *URL, std::string &response_buffer) {
    response_buffer.clear();
    
    // Configure the HTTP client
    esp_http_client_config_t config = {};
    config.url = URL;
    config.event_handler = HttpEventHandler;
    config.user_data = &response_buffer;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 10000;  // increase timeout to 10 seconds
    config.buffer_size = 1024;
    config.buffer_size_tx = 512;
    config.skip_cert_common_name_check = true;
    
    // Retry mechanism
    const int max_retries = 3;
    int retry_count = 0;
    esp_err_t err = ESP_FAIL;
    int status_code = 0;
    
    while (retry_count < max_retries) {
        // Check WiFi connection state
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            ESP_LOGE(TAG, "Wi-Fi is not connected; cannot send HTTP request");
            return -4;  // WiFi not connected error code
        }
        
        ESP_LOGI(TAG, "HTTP request attempt %d/%d: %s", retry_count + 1, max_retries, URL);
        
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));  // exponential backoff
            continue;
        }
        
        // Set HTTP headers
        esp_http_client_set_header(client, "User-Agent", "ESP32-Weather-Client/1.0");
        
        // Send GET request
        err = esp_http_client_perform(client);
        status_code = esp_http_client_get_status_code(client);
        
        if (err == ESP_OK && status_code == 200) {
            // Request succeeded
            esp_http_client_cleanup(client);
            ESP_LOGI(TAG, "HTTP request succeeded; status code: %d", status_code);
            return 0;
        }
        
        // Request failed; log the error and clean up
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "HTTP request failed (attempt %d/%d): %s",
                    retry_count + 1, max_retries, esp_err_to_name(err));
        } else {
            ESP_LOGE(TAG, "HTTP request returned an error status code (attempt %d/%d): %d",
                    retry_count + 1, max_retries, status_code);
        }
        
        esp_http_client_cleanup(client);
        
        // Increase retry interval
        retry_count++;
        if (retry_count < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));  // exponential backoff
        }
    }
    
    // All retries failed
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request ultimately failed: %s", esp_err_to_name(err));
        return -2;
    } else {
        ESP_LOGE(TAG, "HTTP request ultimately returned an error status code: %d", status_code);
        return -3;
    }
}

// Obtain the public IP address
int HTTP_Get_IP(char* read_buf) {
    if (read_buf == nullptr) {
        return 1;
    }
    
    std::string response_buffer;
    ESP_LOGI(TAG, "Starting public IP address lookup");
    
    int res = HTTP_Read_Data(HTTPS_HuoQuIP, response_buffer);
    if (res != 0) {
        ESP_LOGE(TAG, "Failed to get public IP address: %d", res);
        return -1;
    }
    
    // Process the response data, extracting the IP address from the JSON
    if (!response_buffer.empty()) {
        // Parse JSON response
        cJSON *root = cJSON_Parse(response_buffer.c_str());
        if (!root) {
            ESP_LOGE(TAG, "JSON parsing failed");
            return -2;
        }
        
        // Get the IP field
        cJSON *ip = cJSON_GetObjectItem(root, "ip");
        if (!ip || !cJSON_IsString(ip)) {
            ESP_LOGE(TAG, "Failed to get IP field");
            cJSON_Delete(root);
            return -3;
        }
        
        // Copy the IP address into the output buffer
        strncpy(read_buf, ip->valuestring, 30 - 1);
        read_buf[30 - 1] = '\0'; // ensure the string is null-terminated
        
        ESP_LOGI(TAG, "Public IP address: %s", read_buf);
        
        // If a city field is present, the city info can be read directly
        cJSON *city = cJSON_GetObjectItem(root, "city");
        if (city && cJSON_IsString(city)) {
            ESP_LOGI(TAG, "City returned directly by ipinfo.io: %s", city->valuestring);
        }
        
        cJSON_Delete(root);
        return 0;
    }
    
    ESP_LOGE(TAG, "Failed to get public IP address: empty response");
    return -2;
}

// Get city info by IP address
int HTTP_Get_ChengShi(const char* ip_buf, char* read_buf) {
    if (read_buf == nullptr || ip_buf == nullptr) {
        return 1;
    }
    
    std::string response_buffer;
    char url[128];
    
    ESP_LOGI(TAG, "Starting city lookup");
    snprintf(url, sizeof(url), "%s%s", HTTPS_DingWei, ip_buf);
    
    int res = HTTP_Read_Data(url, response_buffer);
    if (res != 0) {
        ESP_LOGE(TAG, "Failed to get city information: %d", res);
        return -1;
    }
    
    // Parse JSON response
    cJSON *root = cJSON_Parse(response_buffer.c_str());
    if (!root) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return -2;
    }
    
    // Check status code
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || strcmp(status->valuestring, "1") != 0) {
        ESP_LOGE(TAG, "Location lookup failed: status code is not 1");
        cJSON_Delete(root);
        return -3;
    }
    
    // Get city info
    cJSON *city = cJSON_GetObjectItem(root, "city");
    if (!city || !cJSON_IsString(city)) {
        ESP_LOGE(TAG, "City lookup returned an empty value");
        cJSON_Delete(root);
        return -4;
    }
    
    // Copy the city name into the output buffer
    strncpy(read_buf, city->valuestring, 30 - 1);
    read_buf[30 - 1] = '\0'; // ensure the string is null-terminated
    
    ESP_LOGI(TAG, "City: %s", read_buf);
    cJSON_Delete(root);
    return 0;
}

// Fetch weather info
int HTTP_Get_TianQi(const char* city_name_buf, _Weather_Data read_buf[3]) {
    if (read_buf == nullptr || city_name_buf == nullptr) {
        return 1;
    }
    
    std::string response_buffer;
    char url[256];
    
    ESP_LOGI(TAG, "Starting weather lookup");
    
    // URL-encode the city name
    std::string encoded_city_str;
    encoded_city_str.reserve(strlen(city_name_buf) * 3); // reserve enough space
    
    for (unsigned char c : std::string(city_name_buf)) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            // Characters that need no encoding
            encoded_city_str += c;
        } else {
            // Characters that need encoding
            encoded_city_str += '%';
            encoded_city_str += "0123456789ABCDEF"[c >> 4]; // high nibble
            encoded_city_str += "0123456789ABCDEF"[c & 0xF]; // low nibble
        }
    }
    
    snprintf(url, sizeof(url), "%s%s", HTTPS_XinZHi, encoded_city_str.c_str());
    
    int res = HTTP_Read_Data(url, response_buffer);
    if (res != 0) {
        ESP_LOGE(TAG, "Failed to get weather data: %d", res);
        return -1;
    }
    
    // Parse JSON response
    cJSON *root = cJSON_Parse(response_buffer.c_str());
    
    // Initialize the struct so every field has a default value
    memset(read_buf, 0, sizeof(_Weather_Data) * 3);
    if (!root) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return -2;
    }
    
    // Get the results array
    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (!results || !cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        ESP_LOGE(TAG, "Invalid weather data format: missing results array");
        cJSON_Delete(root);
        return -3;
    }
    
    // Get the first result
    cJSON *result = cJSON_GetArrayItem(results, 0);
    if (!result) {
        ESP_LOGE(TAG, "Invalid weather data format: missing result item");
        cJSON_Delete(root);
        return -4;
    }
    
    // Get the location object to extract the Chinese city name
    cJSON *location = cJSON_GetObjectItem(result, "location");
    if (location && cJSON_IsObject(location)) {
        cJSON *city_name = cJSON_GetObjectItem(location, "name");
        if (city_name && cJSON_IsString(city_name)) {
            // Save the Chinese city name to a global variable or an output parameter
            // Here the city name could be stored in a field of read_buf, or the incoming city_name_buf could be modified directly
            // But since city_name_buf is const, this must be handled outside the function
            // Here the Chinese city name is stored in a static variable for external use
            static char chinese_city_name[30];
            strncpy(chinese_city_name, city_name->valuestring, sizeof(chinese_city_name) - 1);
            chinese_city_name[sizeof(chinese_city_name) - 1] = '\0';
            ESP_LOGI(TAG, "Retrieved Chinese city name: %s", chinese_city_name);
            
            // Store the Chinese city name in an unused field of read_buf so it can be read externally
            strncpy(read_buf[0].city, city_name->valuestring, sizeof(read_buf[0].city) - 1);
            read_buf[0].city[sizeof(read_buf[0].city) - 1] = '\0';
        }
    }
    
    // Get the now object
    cJSON *now = cJSON_GetObjectItem(result, "now");
    if (!now || !cJSON_IsObject(now)) {
        ESP_LOGE(TAG, "Invalid weather data format: missing now object");
        cJSON_Delete(root);
        return -5;
    }
    
    // Parse the weather data
    cJSON *data;
    
    // Date field removed; the date is no longer fetched or handled
    
    // Weather condition
    data = cJSON_GetObjectItem(now, "text");
    if (data && cJSON_IsString(data)) {
        strncpy(read_buf[0].text, data->valuestring, sizeof(read_buf[0].text) - 1);
        read_buf[0].text[sizeof(read_buf[0].text) - 1] = '\0';
    }
    
    // Weather condition code
    data = cJSON_GetObjectItem(now, "code");
    if (data && cJSON_IsString(data)) {
        strncpy(read_buf[0].code, data->valuestring, sizeof(read_buf[0].code) - 1);
        read_buf[0].code[sizeof(read_buf[0].code) - 1] = '\0';
    }
    
    // Temperature
    data = cJSON_GetObjectItem(now, "temperature");
    if (data && cJSON_IsString(data)) {
        strncpy(read_buf[0].temperature, data->valuestring, sizeof(read_buf[0].temperature) - 1);
        read_buf[0].temperature[sizeof(read_buf[0].temperature) - 1] = '\0';
    }
    
    // Wind direction, wind speed, humidity and other fields removed; these are no longer fetched or handled
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Weather data retrieved successfully");
    return 0;
}

// Main function to fetch weather info
int https_get_TianQi(char* city, _Weather_Data weather_data[3]) {
    int res = 0;
    
    // If the city is empty, get it via the Weather class's GetCityByIP
    if (city[0] == 0) {
        ESP_LOGI(TAG, "City is empty; attempting IP-based city lookup...");
        
        // Fetch IP and city info directly from ipinfo.io
        std::string response_buffer;
        ESP_LOGI(TAG, "Attempting to retrieve public IP and city information: %s", HTTPS_HuoQuIP);
        
        res = HTTP_Read_Data(HTTPS_HuoQuIP, response_buffer);
        if (res != 0) {
            ESP_LOGE(TAG, "Failed to retrieve IP and city information: %d", res);
            return -1;
        }
        
        // Parse JSON response
        cJSON *root = cJSON_Parse(response_buffer.c_str());
        if (!root) {
            ESP_LOGE(TAG, "JSON parsing failed");
            return -1;
        }
        
        // Get the city field
        cJSON *city_json = cJSON_GetObjectItem(root, "city");
        if (!city_json || !cJSON_IsString(city_json) || strlen(city_json->valuestring) == 0) {
            ESP_LOGE(TAG, "Failed to get city field");
            cJSON_Delete(root);
            return -2;
        }
        
        // Copy the city name into the output buffer
        strncpy(city, city_json->valuestring, 30 - 1);
        city[30 - 1] = '\0'; // ensure the string is null-terminated
        
        ESP_LOGI(TAG, "City retrieved via ipinfo.io: %s", city);
        cJSON_Delete(root);
    }
    
    // Fetch weather data
    ESP_LOGI(TAG, "Retrieving weather data for city: %s...", city);
    res = HTTP_Get_TianQi(city, weather_data);
    if (res) {
        ESP_LOGE(TAG, "Failed to retrieve weather: %d", res);
        return -3;
    }
    
    // Print the fetched weather data for debugging
    ESP_LOGI(TAG, "Weather data retrieved successfully for %s", city);
    ESP_LOGI(TAG, "Weather: %s (code: %s)", weather_data[0].text, weather_data[0].code);
    ESP_LOGI(TAG, "Temperature: %s°C", weather_data[0].temperature);
    return 0;
}

// Weather class method implementations

// Obtain the public IP address
std::string Weather::GetPublicIP() {
    // Cache the IP in a global variable to avoid frequent requests
    static std::string cached_ip;
    static bool ip_obtained = false;
    
    // If the IP was already fetched successfully, return the cached IP directly
    if (ip_obtained && !cached_ip.empty()) {
        ESP_LOGI(TAG, "Using previously retrieved public IP: %s", cached_ip.c_str());
        return cached_ip;
    }
    
    // Fetch IP info directly from ipinfo.io
    std::string response_buffer;
    ESP_LOGI(TAG, "Attempting to retrieve public IP: %s", HTTPS_HuoQuIP);
    
    int res = HTTP_Read_Data(HTTPS_HuoQuIP, response_buffer);
    if (res != 0) {
        ESP_LOGE(TAG, "Failed to retrieve IP information: %d", res);
        return "";
    }
    
    // Parse JSON response
    cJSON *root = cJSON_Parse(response_buffer.c_str());
    if (!root) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return "";
    }
    
    // Get the IP field
    cJSON *ip_json = cJSON_GetObjectItem(root, "ip");
    if (!ip_json || !cJSON_IsString(ip_json)) {
        ESP_LOGE(TAG, "Failed to get IP field");
        cJSON_Delete(root);
        return "";
    }
    
    // Update the global cache
    cached_ip = ip_json->valuestring;
    ip_obtained = true; // mark the IP as successfully obtained
    
    ESP_LOGI(TAG, "Successfully retrieved public IP: %s", cached_ip.c_str());
    cJSON_Delete(root);
    return cached_ip;
}
    
// Get city info by public IP address
bool Weather::GetCityByIP() {
    // Cache the last successfully fetched city in a static variable to avoid frequent requests
    static std::string cached_city;
    static std::string city_for_ip;
    
    // If a cached city exists and the IP has not changed, use the cache directly
    if (!cached_city.empty() && !city_for_ip.empty()) {
        ESP_LOGI(TAG, "Using cached city information: %s (IP: %s)", cached_city.c_str(), city_for_ip.c_str());
        city_ = cached_city;
        return true;
    }
    
    // Fetch IP and city info directly from ipinfo.io
    std::string response_buffer;
    ESP_LOGI(TAG, "Attempting to retrieve public IP and city information: %s", HTTPS_HuoQuIP);
    
    int res = HTTP_Read_Data(HTTPS_HuoQuIP, response_buffer);
    if (res != 0) {
        ESP_LOGE(TAG, "Failed to retrieve IP and city information: %d", res);
        return false;
    }
    
    // Parse JSON response
    cJSON *root = cJSON_Parse(response_buffer.c_str());
    if (!root) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return false;
    }
    
    // Get the IP field
    cJSON *ip_json = cJSON_GetObjectItem(root, "ip");
    if (!ip_json || !cJSON_IsString(ip_json)) {
        ESP_LOGE(TAG, "Failed to get IP field");
        cJSON_Delete(root);
        return false;
    }
    
    std::string ip = ip_json->valuestring;
    
    // Get the city field
    cJSON *city_json = cJSON_GetObjectItem(root, "city");
    if (!city_json || !cJSON_IsString(city_json) || strlen(city_json->valuestring) == 0) {
        ESP_LOGE(TAG, "Failed to get city field; trying the Amap API");
        cJSON_Delete(root);
        
        // If ipinfo.io does not return city info, try the Amap (Gaode) API
        char city_buffer[30] = {0};
        int result = HTTP_Get_ChengShi(ip.c_str(), city_buffer);
        
        if (result == 0 && strlen(city_buffer) > 0) {
            // Update the city and cache
            city_ = city_buffer;
            cached_city = city_;
            city_for_ip = ip;
            
            ESP_LOGI(TAG, "City retrieved via Amap API: %s", city_.c_str());
            return true;
        }
        
        ESP_LOGE(TAG, "Failed to retrieve city data via Amap API; error code: %d", result);
        return false;
    }
    
    // Update the city and cache
    city_ = city_json->valuestring;
    cached_city = city_;
    city_for_ip = ip;
    
    ESP_LOGI(TAG, "City retrieved via ipinfo.io: %s (IP: %s)", city_.c_str(), ip.c_str());
    cJSON_Delete(root);
    return true;
}
    
// Update weather data using the implementation in weather_impl.h
void Weather::UpdateWeather() {
    if (is_updating_) {
        return;
    }
    is_updating_ = true;
    
    // Cache the last successfully fetched weather data in a static variable to avoid frequent requests
    static std::string cached_city;
    static float cached_temperature = 0.0f;
    static std::string cached_weather;
    static std::string cached_weather_code;
    static uint32_t last_update_time = 0;
    const uint32_t CACHE_VALID_TIME = 1800000; // cache validity: 30 minutes
    
    // Check whether the cache is valid
    uint32_t current_time = esp_timer_get_time() / 1000; // convert to milliseconds
    if (city_ == cached_city && (current_time - last_update_time < CACHE_VALID_TIME)) {
        ESP_LOGI(TAG, "Using cached weather data: %s, %.1f°C, %s",
                 cached_city.c_str(), cached_temperature, cached_weather.c_str());
        temperature_ = cached_temperature;
        weather_ = cached_weather;
        weather_code_ = cached_weather_code;
        is_updating_ = false;
        return;
    }
    
    // Check WiFi connection state
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi is not connected; cannot retrieve weather data");
        is_updating_ = false;
        return;
    }
    
    // Check whether the city name is empty
    if (city_.empty()) {
        ESP_LOGE(TAG, "City name is empty; using the default city");
        city_ = "武汉市";
    }
    
    // Fetch weather data using https_get_TianQi
    _Weather_Data weather_data[3] = {0};
    char city_buffer[30];
    strncpy(city_buffer, city_.c_str(), sizeof(city_buffer) - 1);
    city_buffer[sizeof(city_buffer) - 1] = '\0';
    
    int result = https_get_TianQi(city_buffer, weather_data);
    
    // Log the weather update result
    if (result != 0) {
        ESP_LOGE(TAG, "Weather update failed; error code: %d", result);
        // If the update fails, use the default values or keep the current ones
        is_updating_ = false;
        return;
    }
    
    // Weather update succeeded; process the data
    // Update member variables
    
    // If the API returned a Chinese city name, use it
    if (strlen(weather_data[0].city) > 0) {
        city_ = weather_data[0].city;
        ESP_LOGI(TAG, "Using Chinese city name returned by the API: %s", city_.c_str());
    } else {
        city_ = city_buffer; // use the possibly updated city name
    }
    
    // Use the weather condition
    if (strlen(weather_data[0].text) > 0) {
        weather_ = weather_data[0].text;
    }
    
    // Convert the temperature string to a float
    if (strlen(weather_data[0].temperature) > 0) {
        temperature_ = atof(weather_data[0].temperature);
    }
    
    // Use the weather code
    if (strlen(weather_data[0].code) > 0) {
        weather_code_ = weather_data[0].code;
    }
    
    ESP_LOGI(TAG, "Parsed weather data: city=%s, weather=%s, temperature=%.1f°C, weather code=%s",
             city_.c_str(), weather_.c_str(), temperature_, weather_code_.c_str());
    
    // Update the cache
    cached_city = city_;
    cached_temperature = temperature_;
    cached_weather = weather_;
    cached_weather_code = weather_code_;
    last_update_time = current_time;
    
    // Set the data-ready flag
    is_data_ready_ = true;
    
    ESP_LOGI(TAG, "Weather update succeeded: %s, %.1f°C, %s, code: %s",
             city_.c_str(), temperature_, weather_.c_str(), weather_code_.c_str());
    
    is_updating_ = false;
}
    
// Weather update task function
void Weather::WeatherUpdateTask(void* arg) {
    Weather* weather = static_cast<Weather*>(arg);
    
    // Set the update flag
    weather->is_updating_ = true;
    
    // Perform the weather update
    weather->UpdateWeather();
    
    // Clear the update flag
    weather->is_updating_ = false;
    
    // Make sure the data is ready
    if (weather->is_data_ready_) {
        // Notify the application to update the UI
        ESP_LOGI(TAG, "Weather data has been updated and is ready; preparing to refresh the weather clock UI");
        
        // Add a short delay to make sure the data is fully ready
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Use the Schedule method to dispatch the task that updates the weather clock UI
        Application::GetInstance().Schedule([]() {
            // Get the Board instance
            Board& board = Board::GetInstance();
            // Check the board type
            if (board.GetBoardType() == "magai-wifi") {
                ESP_LOGI("Weather", "Refreshing the weather clock UI directly");
                // Since the magai_wifi class methods cannot be accessed directly, the update is triggered through a device state change
                // Instead of the previous approach, a simpler one is used:
                // First set a non-idle state, then immediately set it back to idle, which triggers MagaiLed::OnStateChanged
                // and thereby calls magai_wifi::UpdateWeatherClock
                DeviceState current_state = Application::GetInstance().GetDeviceState();
                if (current_state == kDeviceStateIdle) {
                    ESP_LOGI("Weather", "Triggered weather clock refresh through a device state change");
                    Application::GetInstance().SetDeviceState(kDeviceStateStarting);
                    vTaskDelay(pdMS_TO_TICKS(10)); // short delay to ensure the state change is detected
                    Application::GetInstance().SetDeviceState(kDeviceStateIdle);
                }
            }
        });
    } else {
        ESP_LOGW(TAG, "Weather update completed, but data is not ready; UI refresh will not be triggered");
    }
    
    // Delete the task
    vTaskDelete(NULL);
}
    
// Start periodic updates
void Weather::StartPeriodicUpdate() {
    // If the timer already exists, stop and delete it first
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
        update_timer_ = nullptr;
    }
    
    // Configure timer parameters
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = UpdateTimerCallback;
    timer_args.arg = this;
    timer_args.name = "weather_update_timer";
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.skip_unhandled_events = true; // skip unhandled events to avoid a backlog
    
    // Create and start the timer, firing once every 60 minutes
    esp_err_t err = esp_timer_create(&timer_args, &update_timer_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create weather update timer: %s", esp_err_to_name(err));
        return;
    }
    
    err = esp_timer_start_periodic(update_timer_, 3600000000); // 60 minutes, in microseconds
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start weather update timer: %s", esp_err_to_name(err));
        esp_timer_delete(update_timer_);
        update_timer_ = nullptr;
        return;
    }
    
    ESP_LOGI(TAG, "Periodic weather updates started; updating every 60 minutes");
    
    // No longer run an update immediately; instead wait for the device to become idle
    // The initial logic for fetching the public IP and city, and weather by city, has been moved to the magai_wifi class's OnStateChanged
    // This avoids triggering during framework init and causing a stack overflow
}

// Weather class constructor
Weather::Weather() : Thing("Weather", "天气信息"),
               city_("武汉市"), 
               temperature_(25.0f), 
               weather_("晴"), 
               weather_code_("0"), 
               is_updating_(false), 
               is_data_ready_(false), 
               update_timer_(nullptr), 
               gen_(rd_()),
               user_data_(nullptr) {
        // Define the device properties
        properties_.AddStringProperty("city", "城市", [this]() -> std::string {
            return city_;
        });

        properties_.AddNumberProperty("temperature", "温度", [this]() -> float {
            return temperature_;
        });

        properties_.AddStringProperty("weather", "天气状态", [this]() -> std::string {
            return weather_;
        });

        // Define the device methods
        methods_.AddMethod("update", "更新天气", ParameterList(), [this](const ParameterList& /*params*/) {
            UpdateWeather();
        });

        methods_.AddMethod("setCity", "设置城市", ParameterList({
            Parameter("city", "城市名称", kValueTypeString, true)
        }), [this](const ParameterList& params) {
            const Parameter& cityParam = params["city"];
            city_ = cityParam.string();
            UpdateWeather();
        });
        
        methods_.AddMethod("autoDetectCity", "自动检测城市", ParameterList(), [this](const ParameterList& /*params*/) {
            if (AutoDetectCity()) {
                ESP_LOGI(TAG, "Automatically detected city via public IP: %s", city_.c_str());
            } else {
                ESP_LOGE(TAG, "Could not detect city via public IP");
            }
        });

        // Do not initialize weather data in the constructor; wait until the xiaozhi framework has finished initializing and the device is idle
        // This avoids triggering during framework init and causing a stack overflow
        ESP_LOGI(TAG, "Weather service initialized; using default city: %s", city_.c_str());
        ESP_LOGI(TAG, "Initial weather retrieval will run after the Xiaozhi framework initializes (Application: STATE: idle)");
        
        // Periodically check the application state and initialize weather data while idle
        // Create a timer task that periodically checks the application state
        xTaskCreate(
            [](void* arg) {
                Weather* w = static_cast<Weather*>(arg);
                // Wait for the application to enter the idle state
                while (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
                    vTaskDelay(pdMS_TO_TICKS(1000)); // check once per second
                }
                
                ESP_LOGI(TAG, "Application entered idle state; initializing weather data");
                
                // First try to auto-detect the city from the public IP
                if (w->AutoDetectCity()) {
                    ESP_LOGI(TAG, "Automatically detected city via public IP: %s", w->GetCity().c_str());
                } else {
                    ESP_LOGI(TAG, "Could not detect city via public IP; using the default city");
                }
                
                // Update the weather data
                w->UpdateWeather();
                
                vTaskDelete(NULL);
            },
            "weather_init_task",   // task name
            8192,                  // stack size
            this,                  // task argument
            5,                     // task priority
            NULL                   // task handle
        );
        
        // Start periodic updates (without an immediate initial update)
        StartPeriodicUpdate();
    }
    
// Weather class destructor
Weather::~Weather() {
    // Release timer resources
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
        update_timer_ = nullptr;
    }
}

// Auto-detect city
bool Weather::AutoDetectCity() {
    bool success = GetCityByIP();
    if (success) {
        UpdateWeather();
    } else {
        ESP_LOGE(TAG, "Automatic city detection failed; using default city: %s", city_.c_str());
    }
    return success;
}
    
// Set city
void Weather::SetCity(const std::string& city) {
    if (city != city_) {
        city_ = city;
        ESP_LOGI(TAG, "City set to: %s", city_.c_str());
        UpdateWeather();
    }
}
    
// Get current weather data
WeatherData Weather::GetWeatherData() const {
    return WeatherData{
        .city = city_,
        .temperature = temperature_,
        .weather = weather_,
        .weather_code = weather_code_
    };
}

// Convenience accessor (calls GetWeatherData internally)
std::string Weather::GetCity() const { return GetWeatherData().city; }
float Weather::GetTemperature() const { return GetWeatherData().temperature; }
std::string Weather::GetWeather() const { return GetWeatherData().weather; }
std::string Weather::GetWeatherCode() const { return GetWeatherData().weather_code; }
    
/**
 * Get the weather icon object corresponding to the current weather code.
 * Note: built-in icons have been removed; this function now returns nullptr.
 * Use the functions in the weather_display_new module to display weather icons.
 * 
 * @return const lv_img_dsc_t* always returns nullptr
 */
const lv_img_dsc_t* Weather::GetWeatherIconObject() const {
    ESP_LOGW(TAG, "Built-in icons have been removed; use the functions in weather_display_new to display weather icons");
    return nullptr;
}

// Timer callback implementation
void Weather::UpdateTimerCallback(void* arg) {
    Weather* weather = static_cast<Weather*>(arg);
    
    // Check the WiFi connection state to avoid updating weather while disconnected
    wifi_ap_record_t ap_info;
    bool is_wifi_connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
    
    // Track the WiFi connection state with a static variable
    static bool was_wifi_connected = false;
    
    if (!is_wifi_connected) {
        ESP_LOGE(TAG, "Wi-Fi is not connected; skipping weather update");
        was_wifi_connected = false;
        return;
    }
    
    // Detect whether WiFi has just reconnected
    if (!was_wifi_connected && is_wifi_connected) {
        ESP_LOGI(TAG, "Wi-Fi reconnected; resetting public IP lookup state");
        
        // Try to re-fetch the city info
        xTaskCreate(
            [](void* arg) {
                Weather* w = static_cast<Weather*>(arg);
                w->AutoDetectCity();
                vTaskDelete(NULL);
            },
            "city_redetect_task",
            4096,
            weather,
            5,
            NULL
        );
    }
    
    // Update the WiFi connection state
    was_wifi_connected = is_wifi_connected;
    
    // Set a flag to prevent reentrancy
    if (weather->is_updating_) {
        ESP_LOGW(TAG, "Weather update is already in progress; skipping this update");
        return;
    }
    
    // Update weather directly; no longer checks for the top of the hour
    ESP_LOGI(TAG, "Updating weather on schedule");
    
    // Run the update via a task queue instead of directly in the timer callback
    // This avoids long-running work in the timer callback and reduces stack usage
    xTaskCreate(
        [](void* arg) {
            Weather* w = static_cast<Weather*>(arg);
            w->UpdateWeather();
            vTaskDelete(NULL);
        },
        "weather_update",         // task name
        4096,                    // stack size
        weather,                 // task argument
        5,                       // task priority
        NULL                     // task handle
    );
}

} // namespace iot

DECLARE_THING(Weather);