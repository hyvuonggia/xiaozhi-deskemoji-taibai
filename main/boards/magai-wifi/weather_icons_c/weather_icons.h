#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include "lvgl.h"

// Weather icon C array declarations
// Variable names use the icon_ prefix to avoid names starting with a digit

// Declare external variables
extern const lv_image_dsc_t _icon_100;  // Corresponds to the variable in 100.c
extern const lv_image_dsc_t _icon_101;  // Corresponds to the variable in 101.c
extern const lv_image_dsc_t _icon_102;  // Corresponds to the variable in 102.c
extern const lv_image_dsc_t _icon_103;  // Corresponds to the variable in 103.c
extern const lv_image_dsc_t _icon_104;  // Corresponds to the variable in 104.c
extern const lv_image_dsc_t _icon_150;  // Corresponds to the variable in 150.c
extern const lv_image_dsc_t _icon_151;  // Corresponds to the variable in 151.c
extern const lv_image_dsc_t _icon_154;  // Corresponds to the variable in 154.c
extern const lv_image_dsc_t _icon_300;  // Corresponds to the variable in 300.c
extern const lv_image_dsc_t _icon_301;  // Corresponds to the variable in 301.c
extern const lv_image_dsc_t _icon_302;  // Corresponds to the variable in 302.c
extern const lv_image_dsc_t _icon_303;  // Corresponds to the variable in 303.c
extern const lv_image_dsc_t _icon_304;  // Corresponds to the variable in 304.c
extern const lv_image_dsc_t _icon_305;  // Corresponds to the variable in 305.c
extern const lv_image_dsc_t _icon_306;  // Corresponds to the variable in 306.c
extern const lv_image_dsc_t _icon_307;  // Corresponds to the variable in 307.c
extern const lv_image_dsc_t _icon_308;  // Corresponds to the variable in 308.c
extern const lv_image_dsc_t _icon_309;  // Corresponds to the variable in 309.c
extern const lv_image_dsc_t _icon_310;  // Corresponds to the variable in 310.c
extern const lv_image_dsc_t _icon_311;  // Corresponds to the variable in 311.c
extern const lv_image_dsc_t _icon_312;  // Corresponds to the variable in 312.c
extern const lv_image_dsc_t _icon_313;  // Corresponds to the variable in 313.c
extern const lv_image_dsc_t _icon_314;  // Corresponds to the variable in 314.c
extern const lv_image_dsc_t _icon_315;  // Corresponds to the variable in 315.c
extern const lv_image_dsc_t _icon_316;  // Corresponds to the variable in 316.c
extern const lv_image_dsc_t _icon_317;  // Corresponds to the variable in 317.c
extern const lv_image_dsc_t _icon_318;  // Corresponds to the variable in 318.c
extern const lv_image_dsc_t _icon_350;  // Corresponds to the variable in 350.c
extern const lv_image_dsc_t _icon_351;  // Corresponds to the variable in 351.c
extern const lv_image_dsc_t _icon_399;  // Corresponds to the variable in 399.c
extern const lv_image_dsc_t _icon_400;  // Corresponds to the variable in 400.c
extern const lv_image_dsc_t _icon_401;  // Corresponds to the variable in 401.c
extern const lv_image_dsc_t _icon_402;  // Corresponds to the variable in 402.c
extern const lv_image_dsc_t _icon_403;  // Corresponds to the variable in 403.c
extern const lv_image_dsc_t _icon_404;  // Corresponds to the variable in 404.c
extern const lv_image_dsc_t _icon_405;  // Corresponds to the variable in 405.c
extern const lv_image_dsc_t _icon_406;  // Corresponds to the variable in 406.c
extern const lv_image_dsc_t _icon_407;  // Corresponds to the variable in 407.c
extern const lv_image_dsc_t _icon_408;  // Corresponds to the variable in 408.c
extern const lv_image_dsc_t _icon_409;  // Corresponds to the variable in 409.c
extern const lv_image_dsc_t _icon_410;  // Corresponds to the variable in 410.c
extern const lv_image_dsc_t _icon_456;  // Corresponds to the variable in 456.c
extern const lv_image_dsc_t _icon_457;  // Corresponds to the variable in 457.c
extern const lv_image_dsc_t _icon_499;  // Corresponds to the variable in 499.c
extern const lv_image_dsc_t _icon_500;  // Corresponds to the variable in 500.c
extern const lv_image_dsc_t _icon_501;  // Corresponds to the variable in 501.c
extern const lv_image_dsc_t _icon_502;  // Corresponds to the variable in 502.c
extern const lv_image_dsc_t _icon_503;  // Corresponds to the variable in 503.c
extern const lv_image_dsc_t _icon_504;  // Corresponds to the variable in 504.c
extern const lv_image_dsc_t _icon_507;  // Corresponds to the variable in 507.c
extern const lv_image_dsc_t _icon_508;  // Corresponds to the variable in 508.c
extern const lv_image_dsc_t _icon_509;  // Corresponds to the variable in 509.c
extern const lv_image_dsc_t _icon_510;  // Corresponds to the variable in 510.c
extern const lv_image_dsc_t _icon_511;  // Corresponds to the variable in 511.c
extern const lv_image_dsc_t _icon_512;  // Corresponds to the variable in 512.c
extern const lv_image_dsc_t _icon_513;  // Corresponds to the variable in 513.c
extern const lv_image_dsc_t _icon_514;  // Corresponds to the variable in 514.c
extern const lv_image_dsc_t _icon_515;  // Corresponds to the variable in 515.c
extern const lv_image_dsc_t _icon_900;  // Corresponds to the variable in 900.c
extern const lv_image_dsc_t _icon_901;  // Corresponds to the variable in 901.c
extern const lv_image_dsc_t _icon_999;  // Corresponds to the variable in 999.c

// Macros for convenient access
#define WEATHER_ICON_100 (&_icon_100)
#define WEATHER_ICON_101 (&_icon_101)
#define WEATHER_ICON_102 (&_icon_102)
#define WEATHER_ICON_103 (&_icon_103)
#define WEATHER_ICON_104 (&_icon_104)
#define WEATHER_ICON_150 (&_icon_150)
#define WEATHER_ICON_151 (&_icon_151)
#define WEATHER_ICON_154 (&_icon_154)
#define WEATHER_ICON_300 (&_icon_300)
#define WEATHER_ICON_301 (&_icon_301)
#define WEATHER_ICON_302 (&_icon_302)
#define WEATHER_ICON_303 (&_icon_303)
#define WEATHER_ICON_304 (&_icon_304)
#define WEATHER_ICON_305 (&_icon_305)
#define WEATHER_ICON_306 (&_icon_306)
#define WEATHER_ICON_307 (&_icon_307)
#define WEATHER_ICON_308 (&_icon_308)
#define WEATHER_ICON_309 (&_icon_309)
#define WEATHER_ICON_310 (&_icon_310)
#define WEATHER_ICON_311 (&_icon_311)
#define WEATHER_ICON_312 (&_icon_312)
#define WEATHER_ICON_313 (&_icon_313)
#define WEATHER_ICON_314 (&_icon_314)
#define WEATHER_ICON_315 (&_icon_315)
#define WEATHER_ICON_316 (&_icon_316)
#define WEATHER_ICON_317 (&_icon_317)
#define WEATHER_ICON_318 (&_icon_318)
#define WEATHER_ICON_350 (&_icon_350)
#define WEATHER_ICON_351 (&_icon_351)
#define WEATHER_ICON_399 (&_icon_399)
#define WEATHER_ICON_400 (&_icon_400)
#define WEATHER_ICON_401 (&_icon_401)
#define WEATHER_ICON_402 (&_icon_402)
#define WEATHER_ICON_403 (&_icon_403)
#define WEATHER_ICON_404 (&_icon_404)
#define WEATHER_ICON_405 (&_icon_405)
#define WEATHER_ICON_406 (&_icon_406)
#define WEATHER_ICON_407 (&_icon_407)
#define WEATHER_ICON_408 (&_icon_408)
#define WEATHER_ICON_409 (&_icon_409)
#define WEATHER_ICON_410 (&_icon_410)
#define WEATHER_ICON_456 (&_icon_456)
#define WEATHER_ICON_457 (&_icon_457)
#define WEATHER_ICON_499 (&_icon_499)
#define WEATHER_ICON_500 (&_icon_500)
#define WEATHER_ICON_501 (&_icon_501)
#define WEATHER_ICON_502 (&_icon_502)
#define WEATHER_ICON_503 (&_icon_503)
#define WEATHER_ICON_504 (&_icon_504)
#define WEATHER_ICON_507 (&_icon_507)
#define WEATHER_ICON_508 (&_icon_508)
#define WEATHER_ICON_509 (&_icon_509)
#define WEATHER_ICON_510 (&_icon_510)
#define WEATHER_ICON_511 (&_icon_511)
#define WEATHER_ICON_512 (&_icon_512)
#define WEATHER_ICON_513 (&_icon_513)
#define WEATHER_ICON_514 (&_icon_514)
#define WEATHER_ICON_515 (&_icon_515)
#define WEATHER_ICON_900 (&_icon_900)
#define WEATHER_ICON_901 (&_icon_901)
#define WEATHER_ICON_999 (&_icon_999)

// Function declarations
const lv_image_dsc_t* get_weather_icon_c_array(int weather_code);

#endif // WEATHER_ICONS_H