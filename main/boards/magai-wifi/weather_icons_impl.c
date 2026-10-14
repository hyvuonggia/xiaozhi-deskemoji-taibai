/**
 * Weather icon implementation file
 */

#include "lvgl.h"

// Declare weather icons
LV_IMG_DECLARE(weather_icon_sunny);
LV_IMG_DECLARE(weather_icon_cloudy);
LV_IMG_DECLARE(weather_icon_partly_cloudy);
LV_IMG_DECLARE(weather_icon_rainy);
LV_IMG_DECLARE(weather_icon_snowy);
LV_IMG_DECLARE(weather_icon_foggy);
LV_IMG_DECLARE(weather_icon_thunderstorm);

// Mapping table from weather code to icon
const lv_img_dsc_t* weather_code_to_icon[] = {
    &weather_icon_sunny,         // 0 - sunny
    &weather_icon_cloudy,        // 1 - cloudy
    &weather_icon_partly_cloudy, // 2 - partly cloudy
    &weather_icon_rainy,         // 3 - rainy
    &weather_icon_snowy,         // 4 - snowy
    &weather_icon_foggy,         // 5 - foggy
    &weather_icon_thunderstorm   // 6 - thunderstorm
};