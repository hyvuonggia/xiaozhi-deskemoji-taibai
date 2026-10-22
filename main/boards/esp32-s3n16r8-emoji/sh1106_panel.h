#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_types.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_lcd_new_panel_sh1106(esp_lcd_panel_io_handle_t io,
                                   i2c_master_bus_handle_t bus,
                                   int reset_gpio_num,
                                   esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif

// Set while the board rebuilds the bus: the renderer must not touch a device
// handle that is being removed (use-after-free -> crash).
void sh1106_set_recovering(bool recovering);
