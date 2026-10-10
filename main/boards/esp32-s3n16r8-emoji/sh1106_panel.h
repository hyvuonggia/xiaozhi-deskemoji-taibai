#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_lcd_new_panel_sh1106(esp_lcd_panel_io_handle_t io,
                                   int reset_gpio_num,
                                   esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
