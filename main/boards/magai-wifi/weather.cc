#include "iot/thing.h"
#include "../common/board.h"
#include "../common/wifi_board.h"
#include "lvgl.h"
#include "weather.h"  // 包含Weather类的声明
#include "application.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "magai_wifi.h"  // 包含 magai_wifi 类的声明

// 天气图标实现已移除

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

// 获取公网IP地址的URL
#define HTTPS_HuoQuIP           "https://ipinfo.io/json"
// 高德定位API URL
#define HTTPS_DingWei           "http://restapi.amap.com/v3/ip?key=e673da4f70707f787c7b00443211602b&ip="
// 心知天气API URL
#define HTTPS_XinZHi_Key        "S8r2hr2JxTta2E0W8"
#define HTTPS_XinZHi            "https://api.seniverse.com/v3/weather/now.json?key=" HTTPS_XinZHi_Key "&language=zh-Hans&unit=c&location="

// 天气实况数据结构体（优化后只保留必要字段）
typedef struct {
    char text[20];                  // 天气现象
    char code[10];                  // 天气现象代码
    char temperature[10];           // 当前温度
    char city[30];                  // 中文城市名
} _Weather_Data;

namespace iot {

// 全局变量，用于缓存公网IP
static std::string cached_ip;

// HTTP响应回调函数
esp_err_t HttpEventHandler(esp_http_client_event_t *evt) {
    std::string* response_buffer = static_cast<std::string*>(evt->user_data);
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // 将接收到的数据追加到响应缓冲区
            if (evt->data_len > 0) {
                response_buffer->append((char*)evt->data, evt->data_len);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// HTTP请求函数 - 增加重试机制和错误处理
int HTTP_Read_Data(const char *URL, std::string &response_buffer) {
    response_buffer.clear();
    
    // 配置HTTP客户端
    esp_http_client_config_t config = {};
    config.url = URL;
    config.event_handler = HttpEventHandler;
    config.user_data = &response_buffer;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 10000;  // 增加超时时间到10秒
    config.buffer_size = 1024;
    config.buffer_size_tx = 512;
    config.skip_cert_common_name_check = true;
    
    // 重试机制
    const int max_retries = 3;
    int retry_count = 0;
    esp_err_t err = ESP_FAIL;
    int status_code = 0;
    
    while (retry_count < max_retries) {
        // 检查WiFi连接状态
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            ESP_LOGE(TAG, "Wi-Fi is not connected; cannot send HTTP request");
            return -4;  // WiFi未连接错误码
        }
        
        ESP_LOGI(TAG, "HTTP request attempt %d/%d: %s", retry_count + 1, max_retries, URL);
        
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));  // 指数退避
            continue;
        }
        
        // 设置HTTP头
        esp_http_client_set_header(client, "User-Agent", "ESP32-Weather-Client/1.0");
        
        // 发送GET请求
        err = esp_http_client_perform(client);
        status_code = esp_http_client_get_status_code(client);
        
        if (err == ESP_OK && status_code == 200) {
            // 请求成功
            esp_http_client_cleanup(client);
            ESP_LOGI(TAG, "HTTP request succeeded; status code: %d", status_code);
            return 0;
        }
        
        // 请求失败，记录错误并清理
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "HTTP request failed (attempt %d/%d): %s",
                    retry_count + 1, max_retries, esp_err_to_name(err));
        } else {
            ESP_LOGE(TAG, "HTTP request returned an error status code (attempt %d/%d): %d",
                    retry_count + 1, max_retries, status_code);
        }
        
        esp_http_client_cleanup(client);
        
        // 增加重试间隔
        retry_count++;
        if (retry_count < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));  // 指数退避
        }
    }
    
    // 所有重试都失败
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request ultimately failed: %s", esp_err_to_name(err));
        return -2;
    } else {
        ESP_LOGE(TAG, "HTTP request ultimately returned an error status code: %d", status_code);
        return -3;
    }
}

// 获取公网IP地址
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
    
    // 处理响应数据，从JSON中提取IP地址
    if (!response_buffer.empty()) {
        // 解析JSON响应
        cJSON *root = cJSON_Parse(response_buffer.c_str());
        if (!root) {
            ESP_LOGE(TAG, "JSON parsing failed");
            return -2;
        }
        
        // 获取IP字段
        cJSON *ip = cJSON_GetObjectItem(root, "ip");
        if (!ip || !cJSON_IsString(ip)) {
            ESP_LOGE(TAG, "Failed to get IP field");
            cJSON_Delete(root);
            return -3;
        }
        
        // 复制IP地址到输出缓冲区
        strncpy(read_buf, ip->valuestring, 30 - 1);
        read_buf[30 - 1] = '\0'; // 确保字符串以null结尾
        
        ESP_LOGI(TAG, "Public IP address: %s", read_buf);
        
        // 如果有city字段，可以直接获取城市信息
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

// 通过IP地址获取城市信息
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
    
    // 解析JSON响应
    cJSON *root = cJSON_Parse(response_buffer.c_str());
    if (!root) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return -2;
    }
    
    // 检查状态码
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || strcmp(status->valuestring, "1") != 0) {
        ESP_LOGE(TAG, "Location lookup failed: status code is not 1");
        cJSON_Delete(root);
        return -3;
    }
    
    // 获取城市信息
    cJSON *city = cJSON_GetObjectItem(root, "city");
    if (!city || !cJSON_IsString(city)) {
        ESP_LOGE(TAG, "City lookup returned an empty value");
        cJSON_Delete(root);
        return -4;
    }
    
    // 复制城市名称到输出缓冲区
    strncpy(read_buf, city->valuestring, 30 - 1);
    read_buf[30 - 1] = '\0'; // 确保字符串以null结尾
    
    ESP_LOGI(TAG, "City: %s", read_buf);
    cJSON_Delete(root);
    return 0;
}

// 获取天气信息
int HTTP_Get_TianQi(const char* city_name_buf, _Weather_Data read_buf[3]) {
    if (read_buf == nullptr || city_name_buf == nullptr) {
        return 1;
    }
    
    std::string response_buffer;
    char url[256];
    
    ESP_LOGI(TAG, "Starting weather lookup");
    
    // URL编码城市名称
    std::string encoded_city_str;
    encoded_city_str.reserve(strlen(city_name_buf) * 3); // 预留足够空间
    
    for (unsigned char c : std::string(city_name_buf)) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            // 不需要编码的字符
            encoded_city_str += c;
        } else {
            // 需要编码的字符
            encoded_city_str += '%';
            encoded_city_str += "0123456789ABCDEF"[c >> 4]; // 高4位
            encoded_city_str += "0123456789ABCDEF"[c & 0xF]; // 低4位
        }
    }
    
    snprintf(url, sizeof(url), "%s%s", HTTPS_XinZHi, encoded_city_str.c_str());
    
    int res = HTTP_Read_Data(url, response_buffer);
    if (res != 0) {
        ESP_LOGE(TAG, "Failed to get weather data: %d", res);
        return -1;
    }
    
    // 解析JSON响应
    cJSON *root = cJSON_Parse(response_buffer.c_str());
    
    // 初始化结构体，确保所有字段都有默认值
    memset(read_buf, 0, sizeof(_Weather_Data) * 3);
    if (!root) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return -2;
    }
    
    // 获取results数组
    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (!results || !cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        ESP_LOGE(TAG, "Invalid weather data format: missing results array");
        cJSON_Delete(root);
        return -3;
    }
    
    // 获取第一个结果
    cJSON *result = cJSON_GetArrayItem(results, 0);
    if (!result) {
        ESP_LOGE(TAG, "Invalid weather data format: missing result item");
        cJSON_Delete(root);
        return -4;
    }
    
    // 获取location对象以提取中文城市名
    cJSON *location = cJSON_GetObjectItem(result, "location");
    if (location && cJSON_IsObject(location)) {
        cJSON *city_name = cJSON_GetObjectItem(location, "name");
        if (city_name && cJSON_IsString(city_name)) {
            // 将中文城市名保存到全局变量或传出参数
            // 这里我们可以将城市名保存到read_buf的某个字段，或者直接修改传入的city_name_buf
            // 但由于city_name_buf是const，我们需要在函数外部处理
            // 这里我们将中文城市名保存到一个静态变量中，供外部使用
            static char chinese_city_name[30];
            strncpy(chinese_city_name, city_name->valuestring, sizeof(chinese_city_name) - 1);
            chinese_city_name[sizeof(chinese_city_name) - 1] = '\0';
            ESP_LOGI(TAG, "Retrieved Chinese city name: %s", chinese_city_name);
            
            // 将中文城市名保存到read_buf的一个未使用字段中，方便外部获取
            strncpy(read_buf[0].city, city_name->valuestring, sizeof(read_buf[0].city) - 1);
            read_buf[0].city[sizeof(read_buf[0].city) - 1] = '\0';
        }
    }
    
    // 获取now对象
    cJSON *now = cJSON_GetObjectItem(result, "now");
    if (!now || !cJSON_IsObject(now)) {
        ESP_LOGE(TAG, "Invalid weather data format: missing now object");
        cJSON_Delete(root);
        return -5;
    }
    
    // 解析天气数据
    cJSON *data;
    
    // 日期字段已移除，不再需要获取和处理日期
    
    // 天气现象
    data = cJSON_GetObjectItem(now, "text");
    if (data && cJSON_IsString(data)) {
        strncpy(read_buf[0].text, data->valuestring, sizeof(read_buf[0].text) - 1);
        read_buf[0].text[sizeof(read_buf[0].text) - 1] = '\0';
    }
    
    // 天气现象代码
    data = cJSON_GetObjectItem(now, "code");
    if (data && cJSON_IsString(data)) {
        strncpy(read_buf[0].code, data->valuestring, sizeof(read_buf[0].code) - 1);
        read_buf[0].code[sizeof(read_buf[0].code) - 1] = '\0';
    }
    
    // 温度
    data = cJSON_GetObjectItem(now, "temperature");
    if (data && cJSON_IsString(data)) {
        strncpy(read_buf[0].temperature, data->valuestring, sizeof(read_buf[0].temperature) - 1);
        read_buf[0].temperature[sizeof(read_buf[0].temperature) - 1] = '\0';
    }
    
    // 风向、风速、湿度等字段已移除，不再需要获取和处理这些数据
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Weather data retrieved successfully");
    return 0;
}

// 获取天气信息的主函数
int https_get_TianQi(char* city, _Weather_Data weather_data[3]) {
    int res = 0;
    
    // 如果城市为空，通过Weather类的GetCityByIP获取城市
    if (city[0] == 0) {
        ESP_LOGI(TAG, "City is empty; attempting IP-based city lookup...");
        
        // 直接从ipinfo.io获取IP和城市信息
        std::string response_buffer;
        ESP_LOGI(TAG, "Attempting to retrieve public IP and city information: %s", HTTPS_HuoQuIP);
        
        res = HTTP_Read_Data(HTTPS_HuoQuIP, response_buffer);
        if (res != 0) {
            ESP_LOGE(TAG, "Failed to retrieve IP and city information: %d", res);
            return -1;
        }
        
        // 解析JSON响应
        cJSON *root = cJSON_Parse(response_buffer.c_str());
        if (!root) {
            ESP_LOGE(TAG, "JSON parsing failed");
            return -1;
        }
        
        // 获取城市字段
        cJSON *city_json = cJSON_GetObjectItem(root, "city");
        if (!city_json || !cJSON_IsString(city_json) || strlen(city_json->valuestring) == 0) {
            ESP_LOGE(TAG, "Failed to get city field");
            cJSON_Delete(root);
            return -2;
        }
        
        // 复制城市名称到输出缓冲区
        strncpy(city, city_json->valuestring, 30 - 1);
        city[30 - 1] = '\0'; // 确保字符串以null结尾
        
        ESP_LOGI(TAG, "City retrieved via ipinfo.io: %s", city);
        cJSON_Delete(root);
    }
    
    // 获取天气数据
    ESP_LOGI(TAG, "Retrieving weather data for city: %s...", city);
    res = HTTP_Get_TianQi(city, weather_data);
    if (res) {
        ESP_LOGE(TAG, "Failed to retrieve weather: %d", res);
        return -3;
    }
    
    // 打印获取到的天气数据，便于调试
    ESP_LOGI(TAG, "Weather data retrieved successfully for %s", city);
    ESP_LOGI(TAG, "Weather: %s (code: %s)", weather_data[0].text, weather_data[0].code);
    ESP_LOGI(TAG, "Temperature: %s°C", weather_data[0].temperature);
    return 0;
}

// Weather类的方法实现

// 获取公网IP地址
std::string Weather::GetPublicIP() {
    // 使用全局变量缓存IP，避免频繁请求
    static std::string cached_ip;
    static bool ip_obtained = false;
    
    // 如果已经成功获取过IP，直接返回缓存的IP
    if (ip_obtained && !cached_ip.empty()) {
        ESP_LOGI(TAG, "Using previously retrieved public IP: %s", cached_ip.c_str());
        return cached_ip;
    }
    
    // 直接从ipinfo.io获取IP信息
    std::string response_buffer;
    ESP_LOGI(TAG, "Attempting to retrieve public IP: %s", HTTPS_HuoQuIP);
    
    int res = HTTP_Read_Data(HTTPS_HuoQuIP, response_buffer);
    if (res != 0) {
        ESP_LOGE(TAG, "Failed to retrieve IP information: %d", res);
        return "";
    }
    
    // 解析JSON响应
    cJSON *root = cJSON_Parse(response_buffer.c_str());
    if (!root) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return "";
    }
    
    // 获取IP字段
    cJSON *ip_json = cJSON_GetObjectItem(root, "ip");
    if (!ip_json || !cJSON_IsString(ip_json)) {
        ESP_LOGE(TAG, "Failed to get IP field");
        cJSON_Delete(root);
        return "";
    }
    
    // 更新全局缓存
    cached_ip = ip_json->valuestring;
    ip_obtained = true; // 标记已成功获取IP
    
    ESP_LOGI(TAG, "Successfully retrieved public IP: %s", cached_ip.c_str());
    cJSON_Delete(root);
    return cached_ip;
}
    
// 通过公网IP地址获取城市信息
bool Weather::GetCityByIP() {
    // 使用静态变量缓存上次成功获取的城市，避免频繁请求
    static std::string cached_city;
    static std::string city_for_ip;
    
    // 如果已有缓存的城市信息且IP未更新，直接使用缓存
    if (!cached_city.empty() && !city_for_ip.empty()) {
        ESP_LOGI(TAG, "Using cached city information: %s (IP: %s)", cached_city.c_str(), city_for_ip.c_str());
        city_ = cached_city;
        return true;
    }
    
    // 直接从ipinfo.io获取IP和城市信息
    std::string response_buffer;
    ESP_LOGI(TAG, "Attempting to retrieve public IP and city information: %s", HTTPS_HuoQuIP);
    
    int res = HTTP_Read_Data(HTTPS_HuoQuIP, response_buffer);
    if (res != 0) {
        ESP_LOGE(TAG, "Failed to retrieve IP and city information: %d", res);
        return false;
    }
    
    // 解析JSON响应
    cJSON *root = cJSON_Parse(response_buffer.c_str());
    if (!root) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return false;
    }
    
    // 获取IP字段
    cJSON *ip_json = cJSON_GetObjectItem(root, "ip");
    if (!ip_json || !cJSON_IsString(ip_json)) {
        ESP_LOGE(TAG, "Failed to get IP field");
        cJSON_Delete(root);
        return false;
    }
    
    std::string ip = ip_json->valuestring;
    
    // 获取城市字段
    cJSON *city_json = cJSON_GetObjectItem(root, "city");
    if (!city_json || !cJSON_IsString(city_json) || strlen(city_json->valuestring) == 0) {
        ESP_LOGE(TAG, "Failed to get city field; trying the Amap API");
        cJSON_Delete(root);
        
        // 如果ipinfo.io没有返回城市信息，尝试使用高德地图API
        char city_buffer[30] = {0};
        int result = HTTP_Get_ChengShi(ip.c_str(), city_buffer);
        
        if (result == 0 && strlen(city_buffer) > 0) {
            // 更新城市和缓存
            city_ = city_buffer;
            cached_city = city_;
            city_for_ip = ip;
            
            ESP_LOGI(TAG, "City retrieved via Amap API: %s", city_.c_str());
            return true;
        }
        
        ESP_LOGE(TAG, "Failed to retrieve city data via Amap API; error code: %d", result);
        return false;
    }
    
    // 更新城市和缓存
    city_ = city_json->valuestring;
    cached_city = city_;
    city_for_ip = ip;
    
    ESP_LOGI(TAG, "City retrieved via ipinfo.io: %s (IP: %s)", city_.c_str(), ip.c_str());
    cJSON_Delete(root);
    return true;
}
    
// 使用weather_impl.h中的实现更新天气数据
void Weather::UpdateWeather() {
    if (is_updating_) {
        return;
    }
    is_updating_ = true;
    
    // 使用静态变量缓存上次成功获取的天气数据，避免频繁请求
    static std::string cached_city;
    static float cached_temperature = 0.0f;
    static std::string cached_weather;
    static std::string cached_weather_code;
    static uint32_t last_update_time = 0;
    const uint32_t CACHE_VALID_TIME = 1800000; // 缓存有效期30分钟
    
    // 检查缓存是否有效
    uint32_t current_time = esp_timer_get_time() / 1000; // 转换为毫秒
    if (city_ == cached_city && (current_time - last_update_time < CACHE_VALID_TIME)) {
        ESP_LOGI(TAG, "Using cached weather data: %s, %.1f°C, %s",
                 cached_city.c_str(), cached_temperature, cached_weather.c_str());
        temperature_ = cached_temperature;
        weather_ = cached_weather;
        weather_code_ = cached_weather_code;
        is_updating_ = false;
        return;
    }
    
    // 检查WiFi连接状态
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi is not connected; cannot retrieve weather data");
        is_updating_ = false;
        return;
    }
    
    // 检查城市名称是否为空
    if (city_.empty()) {
        ESP_LOGE(TAG, "City name is empty; using the default city");
        city_ = "武汉市";
    }
    
    // 使用https_get_TianQi函数获取天气数据
    _Weather_Data weather_data[3] = {0};
    char city_buffer[30];
    strncpy(city_buffer, city_.c_str(), sizeof(city_buffer) - 1);
    city_buffer[sizeof(city_buffer) - 1] = '\0';
    
    int result = https_get_TianQi(city_buffer, weather_data);
    
    // 记录天气更新结果
    if (result != 0) {
        ESP_LOGE(TAG, "Weather update failed; error code: %d", result);
        // 如果更新失败，使用默认值或保持原值
        is_updating_ = false;
        return;
    }
    
    // 天气更新成功，处理数据
    // 更新成员变量
    
    // 如果API返回了中文城市名，则使用API返回的中文城市名
    if (strlen(weather_data[0].city) > 0) {
        city_ = weather_data[0].city;
        ESP_LOGI(TAG, "Using Chinese city name returned by the API: %s", city_.c_str());
    } else {
        city_ = city_buffer; // 使用可能更新后的城市名称
    }
    
    // 使用天气现象
    if (strlen(weather_data[0].text) > 0) {
        weather_ = weather_data[0].text;
    }
    
    // 将温度字符串转换为浮点数
    if (strlen(weather_data[0].temperature) > 0) {
        temperature_ = atof(weather_data[0].temperature);
    }
    
    // 使用天气代码
    if (strlen(weather_data[0].code) > 0) {
        weather_code_ = weather_data[0].code;
    }
    
    ESP_LOGI(TAG, "Parsed weather data: city=%s, weather=%s, temperature=%.1f°C, weather code=%s",
             city_.c_str(), weather_.c_str(), temperature_, weather_code_.c_str());
    
    // 更新缓存
    cached_city = city_;
    cached_temperature = temperature_;
    cached_weather = weather_;
    cached_weather_code = weather_code_;
    last_update_time = current_time;
    
    // 设置数据就绪标志
    is_data_ready_ = true;
    
    ESP_LOGI(TAG, "Weather update succeeded: %s, %.1f°C, %s, code: %s",
             city_.c_str(), temperature_, weather_.c_str(), weather_code_.c_str());
    
    is_updating_ = false;
}
    
// 天气更新任务函数
void Weather::WeatherUpdateTask(void* arg) {
    Weather* weather = static_cast<Weather*>(arg);
    
    // 设置更新标志
    weather->is_updating_ = true;
    
    // 执行天气更新
    weather->UpdateWeather();
    
    // 清除更新标志
    weather->is_updating_ = false;
    
    // 确保数据已就绪
    if (weather->is_data_ready_) {
        // 通知应用程序更新UI
        ESP_LOGI(TAG, "Weather data has been updated and is ready; preparing to refresh the weather clock UI");
        
        // 增加短暂延迟，确保数据完全准备好
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 使用Schedule方法直接调度更新天气时钟UI的任务
        Application::GetInstance().Schedule([]() {
            // 获取Board实例
            Board& board = Board::GetInstance();
            // 检查板子类型
            if (board.GetBoardType() == "magai-wifi") {
                ESP_LOGI("Weather", "Refreshing the weather clock UI directly");
                // 由于我们无法直接访问 magai_wifi 类的方法，我们需要通过设备状态变化来触发更新
                // 但是我们不使用之前的方法，而是使用一个更简单的方法：
                // 先设置为非空闲状态，然后立即设置回空闲状态，这样会触发 MagaiLed::OnStateChanged
                // 从而调用 magai_wifi::UpdateWeatherClock 方法
                DeviceState current_state = Application::GetInstance().GetDeviceState();
                if (current_state == kDeviceStateIdle) {
                    ESP_LOGI("Weather", "Triggered weather clock refresh through a device state change");
                    Application::GetInstance().SetDeviceState(kDeviceStateStarting);
                    vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延迟，确保状态变化被检测到
                    Application::GetInstance().SetDeviceState(kDeviceStateIdle);
                }
            }
        });
    } else {
        ESP_LOGW(TAG, "Weather update completed, but data is not ready; UI refresh will not be triggered");
    }
    
    // 删除任务
    vTaskDelete(NULL);
}
    
// 启动定时更新
void Weather::StartPeriodicUpdate() {
    // 如果定时器已经存在，先停止并删除
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
        update_timer_ = nullptr;
    }
    
    // 配置定时器参数
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = UpdateTimerCallback;
    timer_args.arg = this;
    timer_args.name = "weather_update_timer";
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.skip_unhandled_events = true; // 跳过未处理的事件，避免积压
    
    // 创建并启动定时器，每60分钟触发一次
    esp_err_t err = esp_timer_create(&timer_args, &update_timer_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create weather update timer: %s", esp_err_to_name(err));
        return;
    }
    
    err = esp_timer_start_periodic(update_timer_, 3600000000); // 60分钟，单位是微秒
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start weather update timer: %s", esp_err_to_name(err));
        esp_timer_delete(update_timer_);
        update_timer_ = nullptr;
        return;
    }
    
    ESP_LOGI(TAG, "Periodic weather updates started; updating every 60 minutes");
    
    // 不再立即执行一次更新，而是等待设备状态变为idle后再更新
    // 初始获取外网IP和城市以及通过城市获取天气数据的逻辑已移至magai_wifi类的OnStateChanged方法中
    // 这样可以避免在框架初始化时触发导致栈溢出
}

// Weather类构造函数
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
        // 定义设备的属性
        properties_.AddStringProperty("city", "城市", [this]() -> std::string {
            return city_;
        });

        properties_.AddNumberProperty("temperature", "温度", [this]() -> float {
            return temperature_;
        });

        properties_.AddStringProperty("weather", "天气状态", [this]() -> std::string {
            return weather_;
        });

        // 定义设备的方法
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

        // 不在构造函数中初始化天气数据，而是等待小智框架初始化完成后在idle状态时进行
        // 这样可以避免在框架初始化时触发导致栈溢出
        ESP_LOGI(TAG, "Weather service initialized; using default city: %s", city_.c_str());
        ESP_LOGI(TAG, "Initial weather retrieval will run after the Xiaozhi framework initializes (Application: STATE: idle)");
        
        // 定期检查应用程序状态，在idle状态时初始化天气数据
        // 创建一个定时器任务，定期检查应用程序状态
        xTaskCreate(
            [](void* arg) {
                Weather* w = static_cast<Weather*>(arg);
                // 等待应用程序进入idle状态
                while (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
                    vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒检查一次
                }
                
                ESP_LOGI(TAG, "Application entered idle state; initializing weather data");
                
                // 先尝试通过公网IP自动获取城市
                if (w->AutoDetectCity()) {
                    ESP_LOGI(TAG, "Automatically detected city via public IP: %s", w->GetCity().c_str());
                } else {
                    ESP_LOGI(TAG, "Could not detect city via public IP; using the default city");
                }
                
                // 更新天气数据
                w->UpdateWeather();
                
                vTaskDelete(NULL);
            },
            "weather_init_task",   // 任务名称
            8192,                  // 堆栈大小
            this,                  // 任务参数
            5,                     // 任务优先级
            NULL                   // 任务句柄
        );
        
        // 启动定时更新（不包含立即执行的初始更新）
        StartPeriodicUpdate();
    }
    
// Weather类析构函数
Weather::~Weather() {
    // 清理定时器资源
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
        update_timer_ = nullptr;
    }
}

// 自动检测城市
bool Weather::AutoDetectCity() {
    bool success = GetCityByIP();
    if (success) {
        UpdateWeather();
    } else {
        ESP_LOGE(TAG, "Automatic city detection failed; using default city: %s", city_.c_str());
    }
    return success;
}
    
// 设置城市
void Weather::SetCity(const std::string& city) {
    if (city != city_) {
        city_ = city;
        ESP_LOGI(TAG, "City set to: %s", city_.c_str());
        UpdateWeather();
    }
}
    
// 获取当前天气数据
WeatherData Weather::GetWeatherData() const {
    return WeatherData{
        .city = city_,
        .temperature = temperature_,
        .weather = weather_,
        .weather_code = weather_code_
    };
}

// 便捷访问方法（内部调用GetWeatherData）
std::string Weather::GetCity() const { return GetWeatherData().city; }
float Weather::GetTemperature() const { return GetWeatherData().temperature; }
std::string Weather::GetWeather() const { return GetWeatherData().weather; }
std::string Weather::GetWeatherCode() const { return GetWeatherData().weather_code; }
    
/**
 * 根据当前天气代码获取对应的天气图标对象。
 * 注意：内置图标已被移除，此函数现在返回nullptr。
 * 请使用weather_display_new模块中的函数来显示天气图标。
 * 
 * @return const lv_img_dsc_t* 始终返回nullptr
 */
const lv_img_dsc_t* Weather::GetWeatherIconObject() const {
    ESP_LOGW(TAG, "Built-in icons have been removed; use the functions in weather_display_new to display weather icons");
    return nullptr;
}

// 定时器回调函数实现
void Weather::UpdateTimerCallback(void* arg) {
    Weather* weather = static_cast<Weather*>(arg);
    
    // 检查WiFi连接状态，避免在WiFi未连接时尝试更新天气
    wifi_ap_record_t ap_info;
    bool is_wifi_connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
    
    // 使用静态变量跟踪WiFi连接状态
    static bool was_wifi_connected = false;
    
    if (!is_wifi_connected) {
        ESP_LOGE(TAG, "Wi-Fi is not connected; skipping weather update");
        was_wifi_connected = false;
        return;
    }
    
    // 检测WiFi是否刚刚重新连接
    if (!was_wifi_connected && is_wifi_connected) {
        ESP_LOGI(TAG, "Wi-Fi reconnected; resetting public IP lookup state");
        
        // 尝试重新获取城市信息
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
    
    // 更新WiFi连接状态
    was_wifi_connected = is_wifi_connected;
    
    // 设置标志，避免重入
    if (weather->is_updating_) {
        ESP_LOGW(TAG, "Weather update is already in progress; skipping this update");
        return;
    }
    
    // 直接更新天气，不再检查是否整点
    ESP_LOGI(TAG, "Updating weather on schedule");
    
    // 使用任务队列执行更新，而不是直接在定时器回调中执行
    // 这样可以避免在定时器回调中执行耗时操作，减少栈使用
    xTaskCreate(
        [](void* arg) {
            Weather* w = static_cast<Weather*>(arg);
            w->UpdateWeather();
            vTaskDelete(NULL);
        },
        "weather_update",         // 任务名称
        4096,                    // 堆栈大小
        weather,                 // 任务参数
        5,                       // 任务优先级
        NULL                     // 任务句柄
    );
}

} // namespace iot

DECLARE_THING(Weather);