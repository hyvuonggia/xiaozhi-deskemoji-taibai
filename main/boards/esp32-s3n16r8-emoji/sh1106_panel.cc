#include "sh1106_panel.h"

#include <stdlib.h>
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_check.h"
#include <atomic>
#include "esp_log.h"
#include "driver/gpio.h"
#include "board_config.h"
#include "lvgl.h"

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
    // Panel brightness = the driver's contrast register (there is no backlight).
    // Lower value = less pixel current = slower OLED ageing. Tune this one number:
    // 0x7F full, 0x4F comfortable, below 0x35 gets washed out in daylight.
    ESP_RETURN_ON_ERROR(send_cmd_data(p, 0x81, 0x4F), TAG, "contrast config failed");
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

// ESP-IDF's I2C panel-I/O waits FOREVER (timeout -1) on a held bus, so a single
// wedged transaction freezes the whole screen while everything else keeps running.
// Our bus has one master (us): idle lines are always high. A low line means the
// bus is wedged - drop the frame right here instead of hanging inside ESP's call.
static bool bus_lines_idle() {
    return gpio_get_level(DISPLAY_I2C_SDA_PIN) == 1 &&
           gpio_get_level(DISPLAY_I2C_SCL_PIN) == 1;
}

static std::atomic<bool> recovering{false};
void sh1106_set_recovering(bool on) { recovering.store(on); }

static esp_err_t panel_draw_bitmap(esp_lcd_panel_t *panel, int xs, int ys, int xe, int ye, const void *data) {
    auto *p = reinterpret_cast<sh1106_panel_t *>(panel);
    if (!data || xs < 0 || ys < 0 || xe > PANEL_WIDTH || ye > PANEL_HEIGHT || xs >= xe || ys >= ye) {
        return ESP_ERR_INVALID_ARG;
    }
    if (recovering.load() || !bus_lines_idle()) {
        // Wedged bus: skip the frame, finish the flush, stay alive. Throttled log.
        static int held_drops = 0;
        if (++held_drops % 50 == 1) {
            ESP_LOGW(TAG, "bus held, dropping frame (x%d)", held_drops);
        }
        lv_display_t *disp = lv_display_get_default();
        if (disp != nullptr) {
            lv_disp_flush_ready(disp);
        }
        return ESP_OK;
    }
    xs += p->x_gap;
    xe += p->x_gap;
    ys += p->y_gap;
    ye += p->y_gap;
    if (xs < 0 || xe > PANEL_WIDTH || ys < 0 || ye > PANEL_HEIGHT) return ESP_ERR_INVALID_ARG;

    const auto *frame = static_cast<const uint8_t *>(data);
    for (int page = ys / 8; page <= (ye - 1) / 8; ++page) {
        // One immediate retry: transient bus noise usually passes on the second try.
        bool ok = false;
        for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
            if (send_cmd(p, 0xB0 | page) != ESP_OK) continue;
            int column = xs + COLUMN_OFFSET;
            if (send_cmd(p, column & 0x0F) != ESP_OK) continue;
            if (send_cmd(p, 0x10 | ((column >> 4) & 0x0F)) != ESP_OK) continue;
            const uint8_t *page_data = frame + page * PANEL_WIDTH + xs;
            if (esp_lcd_panel_io_tx_color(p->io, -1, page_data, xe - xs) != ESP_OK) continue;
            ok = true;
        }
        if (!ok) {
            // A dead frame must never wedge LVGL: the port only marks a flush done via
            // on_color_trans_done, which the panel-I/O layer skips on error, and the
            // port ignores draw_bitmap's return code. So finish the flush here ourselves
            // (dropping this one frame) instead of leaving disp->flushing == 1 forever -
            // that used to freeze taskLVGL holding the LVGL mutex and kill mic, servo
            // and the state machine along with the screen.
            ESP_LOGW(TAG, "page %d failed twice, dropping frame", page);
            lv_display_t *disp = lv_display_get_default();
            if (disp != nullptr) {
                lv_disp_flush_ready(disp);
            }
            return ESP_OK;  // deliberate: the error is already logged, the frame is skipped
        }
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
