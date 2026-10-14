// ---------------------------------------------------------------------------
// EspEyes - port bo mat "esp32-eyes" (Luis Llamas + Playful Technology, GPL/AGPL,
// xem main/boards/face) sang LVGL canvas cho man SH1106 128x64 cua board nay.
// Thuat toan ve (hinh chu nhat + do xien + bo goc elip) giu nguyen theo EyeDrawer.h;
// lop phu: morph muot giua cac kieu, chop mat, nhay 1 mat, lieu mat khi ranh.
// ---------------------------------------------------------------------------
#include "esp_eyes.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <cstring>
#include <algorithm>
#include <esp_heap_caps.h>
#include <cmath>
#include <cstdlib>

#define TAG "EspEyes"

namespace {
struct EyePresetTableRow {
    const char* name;
    EyeShape shape;
};

// 25 kieu lay tu main/boards/face/EyePresets.h (sinh tu dong, xem file .inc)
#include "esp_eyes_presets.inc"

constexpr int PRESET_COUNT = sizeof(EYE_PRESETS) / sizeof(EYE_PRESETS[0]);

// server emotion -> kieu mat
struct EmotionMap { const char* emotion; const char* preset; };
const EmotionMap EMOTION_MAP[] = {
    {"happy", "happy"},        {"laughing", "glee"},      {"funny", "glee"},
    {"sad", "sad"},            {"crying", "sad"},         {"angry", "angry"},
    {"surprised", "surprised"},{"shocked", "awe"},        {"confused", "skeptic"},
    {"thinking", "skeptic_alt"},{"embarrassed", "worried"},{"sleepy", "sleepy"},
    {"cool", "focused"},       {"confident", "focused"},  {"relaxed", "squint"},
    {"loving", "happy"},       {"kissy", "happy"},        {"delicious", "glee"},
    {"silly", "squint_alt"},   {"neutral", "normal"},     {"fear", "scared"},
    {"disgust", "annoyed"},    {"surprise", "surprised"}, {"anger", "angry"},
    {"cry", "sad"},            {"awkward", "worried"},    {"sleep", "sleepy"},
    {"wakeup", "surprised"},
};

uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }
float rand_range(float lo, float hi) { return lo + (hi - lo) * (esp_random() % 1000) / 1000.0f; }
}  // namespace

static const EyeShape* FindPreset(const char* name) {
    if (!name) return &EYE_PRESETS[0].shape;
    for (int i = 0; i < PRESET_COUNT; i++) {
        if (strcasecmp(name, EYE_PRESETS[i].name) == 0) return &EYE_PRESETS[i].shape;
    }
    return nullptr;
}

std::string EspEyes::StyleNames() {
    std::string out;
    for (int i = 0; i < PRESET_COUNT; i++) {
        if (i) out += ", ";
        out += EYE_PRESETS[i].name;
    }
    return out;
}

bool EspEyes::IsValidStyle(const char* name) { return FindPreset(name) != nullptr; }

void EspEyes::Initialize(lv_obj_t* screen) {
    screen_ = screen;
    buf_ = (uint8_t*)heap_caps_malloc((WIDTH * HEIGHT) / 8, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf_) {
        buf_ = (uint8_t*)malloc((WIDTH * HEIGHT) / 8);
    }
    memset(buf_, 0, (WIDTH * HEIGHT) / 8);

    canvas_ = lv_canvas_create(screen_);
    lv_canvas_set_buffer(canvas_, buf_, WIDTH, HEIGHT, LV_COLOR_FORMAT_I1);
    // Quy uoc mau giong emoji_controller (man bi dao): nen = TRANG (bit 1), mat = DEN (bit 0).
    // -> trong man hinh se hien nen toi + mat sang, khong can doi palette.
    lv_obj_align(canvas_, LV_ALIGN_CENTER, 0, 0);

    cur_ = EYE_PRESETS[0].shape;   // normal
    target_ = cur_;
    from_ = cur_;
    next_blink_ms_ = now_ms() + 2500;
    next_look_ms_ = now_ms() + 2000;
    ESP_LOGI(TAG, "EspEyes ready: %d style (%s ...)", PRESET_COUNT, EYE_PRESETS[0].name);
}

void EspEyes::lerpShape(EyeShape& out, const EyeShape& a, const EyeShape& b, float t) {
    auto L = [t](float x, float y) { return (int16_t)lroundf(x + (y - x) * t); };
    out.OffsetX = L(a.OffsetX, b.OffsetX);
    out.OffsetY = L(a.OffsetY, b.OffsetY);
    out.Height = L(a.Height, b.Height);
    out.Width = L(a.Width, b.Width);
    out.Slope_Top = a.Slope_Top + (b.Slope_Top - a.Slope_Top) * t;
    out.Slope_Bottom = a.Slope_Bottom + (b.Slope_Bottom - a.Slope_Bottom) * t;
    out.Radius_Top = L(a.Radius_Top, b.Radius_Top);
    out.Radius_Bottom = L(a.Radius_Bottom, b.Radius_Bottom);
    out.Inverse_Radius_Top = L(a.Inverse_Radius_Top, b.Inverse_Radius_Top);
    out.Inverse_Radius_Bottom = L(a.Inverse_Radius_Bottom, b.Inverse_Radius_Bottom);
    out.Inverse_Offset_Top = L(a.Inverse_Offset_Top, b.Inverse_Offset_Top);
    out.Inverse_Offset_Bottom = L(a.Inverse_Offset_Bottom, b.Inverse_Offset_Bottom);
}

void EspEyes::SetPreset(const char* name, int duration_ms) {
    const EyeShape* p = FindPreset(name);
    if (!p) {
        ESP_LOGW(TAG, "style khong ton tai: %s", name ? name : "(null)");
        return;
    }
    from_ = cur_;
    target_ = *p;
    morph_start_ = now_ms();
    morph_end_ = morph_start_ + (duration_ms > 0 ? duration_ms : 1);
    ESP_LOGI(TAG, "doi kieu mat -> %s (%d ms)", name, duration_ms);
}

void EspEyes::SetPresetForEmotion(const char* emotion, int duration_ms) {
    if (!emotion) return;
    if (strcasecmp(emotion, "winking") == 0) {
        Wink();
        return;
    }
    for (const auto& m : EMOTION_MAP) {
        if (strcasecmp(emotion, m.emotion) == 0) {
            SetPreset(m.preset, duration_ms);
            return;
        }
    }
    // khong map duoc -> ve binh thuong
    SetPreset("normal", duration_ms);
}

void EspEyes::Blink(int duration_ms) {
    blink_start_ = now_ms();
    blink_end_ = blink_start_ + duration_ms;
    blink_left_ = blink_right_ = true;
}

void EspEyes::Wink(bool left_side, int duration_ms) {
    blink_start_ = now_ms();
    blink_end_ = blink_start_ + duration_ms;
    blink_left_ = left_side;
    blink_right_ = !left_side;
}

void EspEyes::Step() {
    const uint32_t now = now_ms();
    frame_++;

    // 1) morph giua 2 kieu
    if (now < morph_end_) {
        float t = (float)(now - morph_start_) / (float)(morph_end_ - morph_start_);
        t = t * t * (3 - 2 * t);  // smoothstep
        lerpShape(cur_, from_, target_, t);
    } else {
        cur_ = target_;
    }

    // 2) chop mat
    if (now < blink_end_ && blink_end_ > blink_start_) {
        float p = (float)(now - blink_start_) / (float)(blink_end_ - blink_start_);
        blink_amt_ = 1.0f - fabsf(2 * p - 1);
    } else {
        blink_amt_ = 0;
    }

    // 3) khi ranh: tu chop mat + lieu mat ngau nhien
    if (idle_look_) {
        if (now > next_blink_ms_) {
            Blink(180 + (esp_random() % 120));
            next_blink_ms_ = now + 3000 + (esp_random() % 4000);
        }
        if (now > next_look_ms_) {
            look_tx_ = rand_range(-11, 11);
            look_ty_ = rand_range(-5, 5);
            next_look_ms_ = now + 2500 + (esp_random() % 3500);
        }
        look_x_ += (look_tx_ - look_x_) * 0.20f;
        look_y_ += (look_ty_ - look_y_) * 0.20f;
    } else {
        look_x_ += (0 - look_x_) * 0.30f;
        look_y_ += (0 - look_y_) * 0.30f;
    }

    Render();
}

// --------------------------- lop ve ---------------------------
void EspEyes::clear() { memset(buf_, 0xFF, (WIDTH * HEIGHT) / 8); }  // nen trang

void EspEyes::px(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    buf_[y * (WIDTH / 8) + (x >> 3)] &= (uint8_t)~(0x80 >> (x & 7));
}

void EspEyes::hline(int x0, int y, int len) {
    for (int i = 0; i < len; i++) px(x0 + i, y);
}

void EspEyes::box(int x0, int y0, int x1, int y1) {
    int l = x0 < x1 ? x0 : x1, r = x0 < x1 ? x1 : x0;
    int t = y0 < y1 ? y0 : y1, b = y0 < y1 ? y1 : y0;
    for (int y = t; y <= b; y++) hline(l, y, r - l + 1);
}

// Tam giac vuong: (x0,y0), (x1,y1), (x1,y0) - giong u8g2.drawTriangle trong ban goc
void EspEyes::tri(int x0, int y0, int x1, int y1) {
    if (x1 == x0) return;
    int dir = x1 > x0 ? 1 : -1;
    int steps = abs(x1 - x0);
    for (int i = 0; i <= steps; i++) {
        int x = x0 + dir * i;
        int yEnd = y0 + (int)lroundf((float)(y1 - y0) * i / steps);
        int t = y0 < yEnd ? y0 : yEnd, b = y0 < yEnd ? yEnd : y0;
        for (int y = t; y <= b; y++) px(x, y);
    }
}

// Goc bo elip: port tu EyeDrawer::FillEllipseCorner (Bresenham)
void EspEyes::ellipseCorner(int corner, int x0, int y0, int rx, int ry) {
    if (rx < 2 || ry < 2) return;
    int32_t x, y;
    int32_t rx2 = rx * rx, ry2 = ry * ry;
    int32_t fx2 = 4 * rx2, fy2 = 4 * ry2;
    int32_t s;
    const bool right = (corner == 0 || corner == 3);  // 0=TR,1=TL,2=BL,3=BR
    const bool top = (corner == 0 || corner == 1);
    for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
        int yy = top ? y0 - y : y0 + y - 1;
        if (right) {
            hline(x0, yy, x);
        } else {
            for (int i = 0; i <= x; i++) px(x0 - i, yy);
        }
        if (s >= 0) { s += fx2 * (1 - y); y--; }
        s += ry2 * ((4 * x) + 6);
    }
    for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
        int yy = top ? y0 - y : y0 + y - 1;
        if (right) {
            hline(x0, yy, x);
        } else {
            for (int i = 0; i <= x; i++) px(x0 - i, yy);
        }
        if (s >= 0) { s += fy2 * (1 - x); x--; }
        s += rx2 * ((4 * y) + 6);
    }
}

void EspEyes::DrawEye(int cx, int cy, const EyeShape& in, float blink_amount) {
    EyeShape c = in;
    c.OffsetX += (int16_t)lroundf(look_x_);
    c.OffsetY += (int16_t)lroundf(look_y_);

    if (blink_amount > 0.01f) {
        float k = 1.0f - 0.88f * blink_amount;
        c.Height = (int16_t)fmaxf(2, lroundf(c.Height * k));
        c.Radius_Top = (int16_t)fmaxf(0, lroundf(c.Radius_Top * k));
        c.Radius_Bottom = (int16_t)fmaxf(0, lroundf(c.Radius_Bottom * k));
        c.Slope_Top *= k;
        c.Slope_Bottom *= k;
    }

    const int32_t delta_y_top = (int32_t)(c.Height * c.Slope_Top / 2.0f);
    const int32_t delta_y_bottom = (int32_t)(c.Height * c.Slope_Bottom / 2.0f);
    const int32_t totalHeight = c.Height + delta_y_top - delta_y_bottom;

    if (c.Radius_Bottom > 0 && c.Radius_Top > 0 &&
        totalHeight - 1 < c.Radius_Bottom + c.Radius_Top) {
        int32_t rt = (int32_t)((float)c.Radius_Top * (totalHeight - 1) / (c.Radius_Bottom + c.Radius_Top));
        int32_t rb = (int32_t)((float)c.Radius_Bottom * (totalHeight - 1) / (c.Radius_Bottom + c.Radius_Top));
        c.Radius_Top = rt;
        c.Radius_Bottom = rb;
    }

    const int32_t TLc_y = cy + c.OffsetY - c.Height / 2 + c.Radius_Top - delta_y_top;
    const int32_t TLc_x = cx + c.OffsetX - c.Width / 2 + c.Radius_Top;
    const int32_t TRc_y = cy + c.OffsetY - c.Height / 2 + c.Radius_Top + delta_y_top;
    const int32_t TRc_x = cx + c.OffsetX + c.Width / 2 - c.Radius_Top;
    const int32_t BLc_y = cy + c.OffsetY + c.Height / 2 - c.Radius_Bottom - delta_y_bottom;
    const int32_t BLc_x = cx + c.OffsetX - c.Width / 2 + c.Radius_Bottom;
    const int32_t BRc_y = cy + c.OffsetY + c.Height / 2 - c.Radius_Bottom + delta_y_bottom;
    const int32_t BRc_x = cx + c.OffsetX + c.Width / 2 - c.Radius_Bottom;

    const int32_t min_c_x = std::min(TLc_x, BLc_x);
    const int32_t max_c_x = std::max(TRc_x, BRc_x);
    const int32_t min_c_y = std::min(TLc_y, TRc_y);
    const int32_t max_c_y = std::max(BLc_y, BRc_y);

    // than mat
    box(min_c_x, min_c_y, max_c_x, max_c_y);
    box(TRc_x, TRc_y, BRc_x + c.Radius_Bottom, BRc_y);
    box(TLc_x - c.Radius_Top, TLc_y, BLc_x, BLc_y);
    box(TLc_x, TLc_y - c.Radius_Top, TRc_x, TRc_y);
    box(BLc_x, BLc_y, BRc_x, BRc_y + c.Radius_Bottom);

    // do xien tren/duoi
    if (c.Slope_Top > 0) {
        tri(TLc_x, TLc_y - c.Radius_Top, TRc_x, TRc_y - c.Radius_Top);
        tri(TRc_x, TRc_y - c.Radius_Top, TLc_x, TLc_y - c.Radius_Top);
    } else if (c.Slope_Top < 0) {
        tri(TRc_x, TRc_y - c.Radius_Top, TLc_x, TLc_y - c.Radius_Top);
        tri(TLc_x, TLc_y - c.Radius_Top, TRc_x, TRc_y - c.Radius_Top);
    }
    if (c.Slope_Bottom > 0) {
        tri(BRc_x + c.Radius_Bottom, BRc_y + c.Radius_Bottom, BLc_x - c.Radius_Bottom, BLc_y + c.Radius_Bottom);
        tri(BLc_x - c.Radius_Bottom, BLc_y + c.Radius_Bottom, BRc_x + c.Radius_Bottom, BRc_y + c.Radius_Bottom);
    } else if (c.Slope_Bottom < 0) {
        tri(BLc_x - c.Radius_Bottom, BLc_y + c.Radius_Bottom, BRc_x + c.Radius_Bottom, BRc_y + c.Radius_Bottom);
        tri(BRc_x + c.Radius_Bottom, BRc_y + c.Radius_Bottom, BLc_x - c.Radius_Bottom, BLc_y + c.Radius_Bottom);
    }

    // bo goc
    if (c.Radius_Top > 0) {
        ellipseCorner(1, TLc_x, TLc_y, c.Radius_Top, c.Radius_Top);   // TL
        ellipseCorner(0, TRc_x, TRc_y, c.Radius_Top, c.Radius_Top);   // TR
    }
    if (c.Radius_Bottom > 0) {
        ellipseCorner(2, BLc_x, BLc_y, c.Radius_Bottom, c.Radius_Bottom);  // BL
        ellipseCorner(3, BRc_x, BRc_y, c.Radius_Bottom, c.Radius_Bottom);  // BR
    }
}

void EspEyes::Render() {
    if (!canvas_ || !buf_) return;
    if (lv_screen_active() != screen_) return;   // dang o man khac -> khong ton cong ve/I2C
    clear();
    DrawEye(LEFT_CX, EYE_CY, cur_, blink_left_ ? blink_amt_ : 0);
    DrawEye(RIGHT_CX, EYE_CY, cur_, blink_right_ ? blink_amt_ : 0);
    lv_obj_invalidate(canvas_);
}
