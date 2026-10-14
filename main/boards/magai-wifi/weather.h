#pragma once

#include <string>
#include <ctime>
#include <random>
#include "iot/thing.h"
#include "lvgl.h"
#include <esp_http_client.h>

namespace iot {

// Weather state enum
enum class WeatherState {
    SUNNY,
    CLOUDY,
    PARTLY_CLOUDY,
    RAINY,
    SNOWY,
    FOGGY,
    THUNDERSTORM
};

// Weather text mapping
const std::unordered_map<WeatherState, std::string> kWeatherTexts = {
    {WeatherState::SUNNY, "晴"},
    {WeatherState::CLOUDY, "多云"},
    {WeatherState::PARTLY_CLOUDY, "局部多云"},
    {WeatherState::RAINY, "雨"},
    {WeatherState::SNOWY, "雪"},
    {WeatherState::FOGGY, "雾"},
    {WeatherState::THUNDERSTORM, "雷暴"}
};

// Weather data struct
struct WeatherData {
    std::string city;
    float temperature;
    std::string weather;
    std::string weather_code;
};

// Weather class, inherits from Thing
class Weather : public Thing {
public:
    Weather();
    ~Weather();
    
    // Update the weather information (main method)
    void UpdateWeather();
    
    // Auto-detect the city and update the weather
    bool AutoDetectCity();
    
    // Set the city and update the weather
    void SetCity(const std::string& city);
    
    // Get the weather data
    WeatherData GetWeatherData() const;
    
    // Get the weather icon object
    const lv_img_dsc_t* GetWeatherIconObject() const;
    
    // Convenience accessors (call GetWeatherData internally)
    std::string GetCity() const;
    float GetTemperature() const;
    std::string GetWeather() const;
    std::string GetWeatherCode() const;
    
private:
    // Get the city information via IP
    bool GetCityByIP();
    
    // Get the public IP address
    std::string GetPublicIP();
    
    // Start the periodic update
    void StartPeriodicUpdate();
    
    // Timer callback function
    static void UpdateTimerCallback(void* arg);
    
    // Weather update task function
    static void WeatherUpdateTask(void* arg);
    
public:
    // Set user data (used for callbacks)
    void SetUserData(void* user_data) { user_data_ = user_data; }
    
    // Get user data
    void* GetUserData() const { return user_data_; }
    
    // Check whether the weather data is ready
    bool IsDataReady() const { return is_data_ready_; }
    
    // Member variables
    std::string city_;
    float temperature_;
    std::string weather_;
    std::string weather_code_; // Weather code
    bool is_updating_;
    bool is_data_ready_; // Flag whether the weather data is ready to be displayed
    esp_timer_handle_t update_timer_;
    std::random_device rd_;
    std::mt19937 gen_;
    void* user_data_; // User data pointer, used for callbacks
};

} // namespace iot