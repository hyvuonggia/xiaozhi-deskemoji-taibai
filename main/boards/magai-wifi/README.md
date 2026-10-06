# Magai-WiFi Module

## Overview

Magai-WiFi is an ESP32-based smart device module integrating Wi-Fi connectivity, weather display, touch controls, and LED effects. It primarily provides a smart voice-assistant interface and automatically switches to a weather-clock display when idle.

## Hardware Specifications

- **Main chip**：ESP32 series
- **Display**：NV303b LCDDisplay (240x280resolution)
- **Buttons**：
  - Touch button (GPIO 0)
  - Volume-up button (GPIO 1)
  - Volume-down button (GPIO 43)
- **LED**：12 built-in LEDs (GPIO 2)
- **Audio interface**：I2S interface
  - Microphone：GPIO 4(WS), 5(SCK), 6(DIN)
  - Speaker：GPIO 7(DOUT), 15(BCLK), 16(LRCK)
- **Display interface**：
  - 8-bit parallel data bus (GPIO 14, 21, 47, 48, 45, 38, 39, 40)
  - Control signals：GPIO 8(PCLK), 13(DC)
  - Backlight control：GPIO 44
- **I2C interface**：GPIO 18(SDA), 17(SCL)

## Software Features

### 1. Weather Clock

- **Automatic city detection**：Automatically detects the current city using the public IP address
- **Chinese city-name display**：Extracts and displays the Chinese city name from the Seniverse Weather API response
- **Weather data retrieval**：Retrieves real-time weather information using the Seniverse Weather API
- **Scheduled updates**：Automatically refreshes weather data every 60 minutes
- **Idle-mode switching**：Automatically switches to weather-clock display while idle
- **Smart weather icon display**：
  - Loads PNG weather icons using a memory-mapped filesystem
- Supports automatic weather-code mapping (for example, code 9 maps to the cloudy icon 154.png)
  - Icons scale to 32 × 32 pixels
  - Uses the default sunny icon if loading fails
  - Supports real-time weather-code updates and icon changes

### 2. Voice Interaction

- **Touch controls**：Press the touch button to start voice listening; release it to stop
- **Volume control**：Adjusts device volume using the volume buttons
  - Short press: increase/decrease volume by 10%
  - Long press: set volume to maximum/minimum

### 3. IoT Features

- **Device management**：Integrates virtual devices such as Speaker, Lamp, and Screen
- **Status monitoring**：Monitors device state changes and triggers corresponding actions

## Code Structure

### Main Classes

1. **NV303bDisplay**：Display control class
   - Inherits fromSpiLcdDisplay
   - Creates and updates the weather-clock UI
   - Manages UI element visibility

2. **magai_wifi**：Board class
   - Inherits fromWifiBoard
   - Initializes hardware components
   - Manages device states and mode switching
   - Contains the MagaiLed inner class for handling LED state changes

3. **Weather**：Weather service class
   - Inherits fromThing
   - Retrieves and updates weather data
   - Supports automatic city detection and scheduled updates
   - Extracts and displays the Chinese city name from the API response

4. **WeatherDisplayNew**：Weather icon display module
   - Manages weather icon creation, loading, and display
   - Maps weather codes to icon files
   - Uses a memory-mapped filesystem to optimize icon loading
   - Manages icon visibility and positioning

### Key Files

- **magai_wifi.cc**：Main implementation file for NV303bDisplay and magai_wifi
- **weather.h/cc**：Weather service implementation
- **weather_display_new.h/c**：New weather icon display module implementation
- **config.h**：Hardware configuration and pin definitions
- **weather directory**：Directory containing PNG weather icons

## Usage

### Initialization

1. Creating a magai_wifi instance automatically performs the following initialization:
   - Initializes buttons and callbacks
   - Initializes IoT devices and the weather service
   - Initializes the I2C bus
   - Initializes the NV303b display
   - Restores the backlight brightness
   - Checks initial device states and updates the UI

### Weather-clock mode

- Automatically switches to weather-clock mode while idle
- Displays the current city, time, temperature, and weather conditions
- Automatically returns to the normal UI when the device leaves idle state

### Handling state changes

- Monitors device state changes through the MagaiLed class OnStateChanged method
- Automatically refreshes weather data when the state becomes idle
- Updates the weather-clock UI

## Maintenance Notes

1. **Weather API**：To change the weather API, update the relevant URLs and parsing logic in weather.cc
2. **City detection**：City detection depends on a public-IP service; provide an alternative if the service is unavailable
3. **Chinese city name**：The Chinese city name comes from the location.name field in the Seniverse Weather API response; adjust parsing if the API response format changes
4. **UI customization**：Edit the weather-clock UI layout and style in NV303bDisplay::SetupWeatherClockUI
5. **Scheduled updates**：Adjust the weather refresh interval in Weather::StartPeriodicUpdate
6. **Memory management**：Release dynamically allocated resources in the destructor to avoid memory leaks
7. **Weather icon management**：
   - Weather icon files are stored as PNGs in the memory-mapped partition
   - Weather-code mapping is implemented in weather_icon_new_update in weather_display_new.c
   - To add an icon, place its PNG file in the weather directory
   - Adjust icon size and position in weather_display_new.c and magai_wifi.cc

## Troubleshooting

- **Weather data retrieval fails**：Check the Wi-Fi connection and API availability
- **City name displays incorrectly**：Check the JSON response from the Seniverse Weather API and confirm that location.name exists
- **Weather icons do not display**：
  - Check that the memory-mapped filesystem initialized correctly
  - Confirm that weather-code mapping is correct (check the mapping details in the log)
  - Check that the icon files exist in the memory-mapped partition
  - Verify that the icon object visibility flags are set correctly
- **Display behaves incorrectly**：Check the display initialization parameters and pin configuration
- **Buttons do not respond**：Check the button pin configuration and callback registration
- **LEDs do not work**：Check the LED pin configuration and count
- **Compilation errors**：Ensure printf format specifiers match the argument types; use appropriate type conversions

## Future Improvements

1. Add more weather data (such as humidity and wind speed)
2. Improve weather icons and UI layout
3. Add a user-defined city setting
4. Add multilingual support
5. Improve network connection and data retrieval reliability
6. Improve localized city-name display to support more regional languages