#include "sh1106_panel.h"

#include <stdlib.h>
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "sh1106_panel";
static constexpr int PANEL_WIDTH = 128;
static constexpr int PANEL_HEIGHT = 64;
static constexpr int COLUMN_OFFSET = 2;

struct sh1106_panel_t {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int x_gap;
    int y_gap;
};

static esp_err_t panel_reset(esp_lcd_panel_t *) { return ESP_OK; }

static esp_err_t send_cmd(sh1106_panel_t *p, int cmd) {
    return esp_lcd_panel_io_tx_param(p->io, cmd, nullptr, 0);
}

static esp_err_t send_cmd_data(sh1106_panel_t *p, int cmd, uint8_t data) {
    return esp_lcd_panel_io_tx_param(p->io, cmd, &data, 1);
}

static esp_err_t panel_init(esp_lcd_panel_t *panel) {
    auto *p = reinterpret_cast<sh1106_panel_t *>(panel);
    ESP_RETURN_ON_ERROR(send_cmd(p, 0xAE), TAG, "display off failed");
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0xD5, 0x80), TAG, "clock config failed");
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0xA8, 0x3F), TAG, "multiplex config failed");
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0xD3, 0x00), TAG, "display offset config failed");
    ESP_RETURN_ON_ERROR(send_cmd(p, 0x40), TAG, "start line config failed");
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0xAD, 0x8B), TAG, "SH1106 DC-DC config failed");
    ESP_RETURN_ON_ERROR(send_cmd(p, 0xA1), TAG, "segment remap failed");
    ESP_RETURN_ON_ERROR(send_cmd(p, 0xC8), TAG, "COM scan config failed");
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0xDA, 0x12), TAG, "COM pin config failed");
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0x81, 0x7F), TAG, "contrast config failed");
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0xD9, 0xF1), TAG, "pre-charge config failed");
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0xDB, 0x40), TAG, "VCOMH config failed");
    ESP_RETURN_ON_ERROR(send_cmd(p, 0xA4), TAG, "RAM display mode config failed");
    ESP_RETURN_ON_ERROR(send_cmd(p, 0xA6), TAG, "normal display mode config failed");
    ESP_LOGI(TAG, "SH1106-compatible page-addressed panel initialized");
    return ESP_OK;
}

static esp_err_t panel_del(esp_lcd_panel_t *panel) {
    free(reinterpret_cast<sh1106_panel_t *>(panel));
    return ESP_OK;
}

static esp_err_t panel_draw_bitmap(esp_lcd_panel_t *panel, int xs, int ys, int xe, int ye, const void *data) {
    auto *p = reinterpret_cast<sh1106_panel_t *>(panel);
    if (!data || xs < 0 || ys < 0 || xe > PANEL_WIDTH || ye > PANEL_HEIGHT || xs >= xe || ys >= ye) {
        return ESP_ERR_INVALID_ARG;
    }
    xs += p->x_gap;
    xe += p->x_gap;
    ys += p->y_gap;
    ye += p->y_gap;
    if (xs < 0 || xe > PANEL_WIDTH || ys < 0 || ye > PANEL_HEIGHT) return ESP_ERR_INVALID_ARG;

    const auto *frame = static_cast<const uint8_t *>(data);
    for (int page = ys / 8; page <= (ye - 1) / 8; ++page) {
        ESP_RETURN_ON_ERROR(send_cmd(p, 0xB0 | page), TAG, "set page failed");
        int column = xs + COLUMN_OFFSET;
        ESP_RETURN_ON_ERROR(send_cmd(p, column & 0x0F), TAG, "set column low failed");
        ESP_RETURN_ON_ERROR(send_cmd(p, 0x10 | ((column >> 4) & 0x0F)), TAG, "set column high failed");
        const uint8_t *page_data = frame + page * PANEL_WIDTH + xs;
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(p->io, -1, page_data, xe - xs), TAG, "write page data failed");
    }
    return ESP_OK;
}

static esp_err_t panel_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y) {
    auto *p = reinterpret_cast<sh1106_panel_t *>(panel);
    ESP_RETURN_ON_ERROR(send_cmd(p, mirror_x ? 0xA1 : 0xA0), TAG, "set horizontal mirror failed");
    return send_cmd(p, mirror_y ? 0xC8 : 0xC0);
}

static esp_err_t panel_swap_xy(esp_lcd_panel_t *, bool swap) {
    return swap ? ESP_ERR_NOT_SUPPORTED : ESP_OK;
}

static esp_err_t panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap) {
    auto *p = reinterpret_cast<sh1106_panel_t *>(panel);
    p->x_gap = x_gap;
    p->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_invert(esp_lcd_panel_t *panel, bool invert) {
    return send_cmd(reinterpret_cast<sh1106_panel_t *>(panel), invert ? 0xA7 : 0xA6);
}

static esp_err_t panel_on_off(esp_lcd_panel_t *panel, bool on) {
    return send_cmd(reinterpret_cast<sh1106_panel_t *>(panel), on ? 0xAF : 0xAE);
}

extern "C" esp_err_t esp_lcd_new_panel_sh1106(esp_lcd_panel_io_handle_t io,
                                               int reset_gpio_num,
                                               esp_lcd_panel_handle_t *ret_panel) {
    if (!io || !ret_panel) return ESP_ERR_INVALID_ARG;
    auto *p = static_cast<sh1106_panel_t *>(calloc(1, sizeof(sh1106_panel_t)));
    if (!p) return ESP_ERR_NO_MEM;
    p->io = io;
    p->reset_gpio_num = reset_gpio_num;
    p->base.reset = panel_reset;
    p->base.init = panel_init;
    p->base.del = panel_del;
    p->base.draw_bitmap = panel_draw_bitmap;
    p->base.mirror = panel_mirror;
    p->base.swap_xy = panel_swap_xy;
    p->base.set_gap = panel_set_gap;
    p->base.invert_color = panel_invert;
    p->base.disp_on_off = panel_on_off;
    p->base.disp_sleep = nullptr;
    p->base.set_brightness = nullptr;
    p->base.user_data = nullptr;
    *ret_panel = &p->base;
    return ESP_OK;
}
