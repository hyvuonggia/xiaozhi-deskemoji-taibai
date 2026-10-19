// ---------------------------------------------------------------------------
// RobotEyes - OliEyes eye engine (Oliray-ka/OliEyes, MIT) ported to ESP-IDF/LVGL.
// Reference: examples/SH1106_i12C.ino (getMoodTarget, drawEye, updateLook,
// updateBlink, updateWinks). Keeps the 7 shape parameters, the 20 moods and the
// behaviours (mood-based blinking, single-eye wink, double-take, idle look, laughing bounce).
// NOTE the panel is colour-inverted: background = bit 1 (white in LVGL -> black on screen),
// eye shape = bit 0 (black in LVGL -> lit pixel on screen).
// ---------------------------------------------------------------------------
#include "robot_eyes.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <cmath>
#include <algorithm>

#define TAG "RobotEyes"

namespace {
const char* kMoodNames[] = {
    "neutral", "happy", "sad", "angry", "surprised", "sleepy", "love", "suspicious",
    "confused", "scared", "laugh", "bored", "excited", "shy", "annoyed", "dizzy",
    "sick", "proud", "frustrated", "curious",
};
constexpr int MOOD_COUNT = sizeof(kMoodNames) / sizeof(kMoodNames[0]);

enum MoodIdx {
    M_NEUTRAL = 0, M_HAPPY, M_SAD, M_ANGRY, M_SURPRISED, M_SLEEPY, M_LOVE, M_SUSPICIOUS,
    M_CONFUSED, M_SCARED, M_LAUGH, M_BORED, M_EXCITED, M_SHY, M_ANNOYED, M_DIZZY,
    M_SICK, M_PROUD, M_FRUSTRATED, M_CURIOUS,
};

struct EmotionMap { const char* emotion; int mood; };
const EmotionMap EMOTION_MAP[] = {
    {"happy", M_HAPPY},        {"laughing", M_LAUGH},     {"funny", M_LAUGH},
    {"sad", M_SAD},            {"crying", M_SAD},         {"angry", M_ANGRY},
    {"surprised", M_SURPRISED},{"shocked", M_SCARED},     {"confused", M_CONFUSED},
    {"thinking", M_SUSPICIOUS},{"embarrassed", M_SHY},    {"sleepy", M_SLEEPY},
    {"cool", M_PROUD},         {"confident", M_PROUD},    {"relaxed", M_BORED},
    {"loving", M_LOVE},        {"kissy", M_LOVE},         {"delicious", M_EXCITED},
    {"silly", M_DIZZY},        {"neutral", M_NEUTRAL},    {"fear", M_SCARED},
    {"disgust", M_ANNOYED},    {"curiosity", M_CURIOUS},  {"surprise", M_SURPRISED},
    {"anger", M_ANGRY},        {"cry", M_SAD},            {"awkward", M_SHY},
    {"sleep", M_SLEEPY},       {"excited", M_EXCITED},    {"frustrated", M_FRUSTRATED},
};

uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }
int    rand_int(int lo, int hi) { return lo + (int)(esp_random() % (uint32_t)(hi - lo + 1)); }
float ease_to(float cur, float target, float speed) { return cur + (target - cur) * speed; }
}  // namespace

std::string RobotEyes::StyleNames() {
    std::string out;
    for (int i = 0; i < MOOD_COUNT; i++) {
        if (i) out += ", ";
        out += kMoodNames[i];
    }
    return out;
}

bool RobotEyes::IsValidStyle(const char* name) {
    if (!name) return false;
    for (int i = 0; i < MOOD_COUNT; i++) {
        if (strcasecmp(name, kMoodNames[i]) == 0) return true;
    }
    return false;
}

void RobotEyes::Initialize(lv_obj_t* screen) {
    screen_ = screen;
    const size_t bits = (WIDTH * HEIGHT) / 8;
    buf_ = (uint8_t*)heap_caps_malloc(bits, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf_) buf_ = (uint8_t*)malloc(bits);
    memset(buf_, 0xFF, bits);
    // Previous-frame copy for change detection (1 KB); 0x00 forces the first frame
    // to always count as changed so something is drawn at startup.
    pbuf_ = (uint8_t*)heap_caps_malloc(bits, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pbuf_) pbuf_ = (uint8_t*)malloc(bits);
    if (pbuf_) memset(pbuf_, 0x00, bits);

    // RGB565 canvas on purpose, NOT an indexed (I1) one: an indexed canvas keeps its
    // palette INSIDE the buffer (LVGL writes the palette at offset 0 of the very buffer
    // handed to lv_canvas_set_buffer), so a plain pixel buffer is silently shifted by the
    // palette size and the panel ends up showing one flat colour - i.e. blank. RGB565
    // needs no palette and is the format the rest of the render pipeline already uses.
    const size_t px = (size_t)WIDTH * HEIGHT * sizeof(uint16_t);
    rgb_ = (uint16_t*)heap_caps_malloc(px, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rgb_) rgb_ = (uint16_t*)malloc(px);
    memset(rgb_, 0xFF, px);

    canvas_ = lv_canvas_create(screen_);
    lv_canvas_set_buffer(canvas_, rgb_, WIDTH, HEIGHT, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(canvas_, LV_ALIGN_CENTER, 0, 0);

    left_  = {40, 40, 10, 0, 0, 0, 0};
    right_ = {40, 40, 10, 0, 0, 0, 0};
    mood_ = M_NEUTRAL;
    last_look_change_ = now_ms();
    last_blink_ = now_ms();
    last_mood_switch_ = now_ms();
    next_blink_interval_ = 3000;
    ESP_LOGI(TAG, "RobotEyes (OliEyes) ready: %d mood", MOOD_COUNT);
}

// ------------------------- per-mood shape targets (unchanged from OliEyes) -------------------------
void RobotEyes::GetMoodTarget(int mood, bool is_right, EyeTarget& t) const {
    const float R = is_right ? 1.0f : 0.0f;
    switch (mood) {
        case M_HAPPY:      t = {40, 40, 10, 0, 0, 16, 0}; break;
        case M_SAD:        t = {36, 34, 10, 0, 14, 0, 4}; break;
        case M_ANGRY:      t = {36, 30, 8, 16, 0, 0, 2}; break;
        case M_SURPRISED:  t = {46, 50, 20, 0, 0, 0, -2}; break;
        case M_SLEEPY:     t = {40, 14, 6, 0, 0, 0, 10}; break;
        case M_LOVE:       t = {40, 40, 10, 0, 0, 19, -2}; break;
        case M_SUSPICIOUS: t = {40, 20, 6, 0, R * 10.0f, 0, 2}; break;
        case M_CONFUSED:   t = {40, 40, 10, 0, 0, 0, is_right ? -8.0f : 8.0f}; break;
        case M_SCARED:     t = {44, 48, 18, 0, 0, 0, -4}; break;
        case M_LAUGH:      t = {42, 42, 10, 0, 0, 26, -2}; break;
        case M_BORED:      t = {40, 16, 8, 0, 6, 0, 8}; break;
        case M_EXCITED:    t = {44, 46, 18, 0, 0, 0, -3}; break;
        case M_SHY:        t = {32, 24, 10, 0, 0, 8, 8}; break;
        case M_ANNOYED:    t = {38, 22, 8, 10, 0, 0, 2}; break;
        case M_DIZZY:      t = {38, 38, 14, 0, 0, 0, 0}; break;
        case M_SICK:       t = {38, is_right ? 24.0f : 34.0f, 10, 0, 6, 0, 6}; break;
        case M_PROUD:      t = {42, 44, 14, 0, 0, 10, -3}; break;
        case M_FRUSTRATED: t = {34, 26, 8, 20, 0, 0, 4}; break;
        case M_CURIOUS:    t = {is_right ? 46.0f : 34.0f, is_right ? 46.0f : 34.0f, 14, 0, 0, 0,
                                is_right ? -2.0f : 2.0f}; break;
        case M_NEUTRAL:
        default:           t = {40, 40, 10, 0, 0, 0, 0}; break;
    }
}

// ------------------------- drawing primitives -------------------------
void RobotEyes::clear() { memset(buf_, 0xFF, (WIDTH * HEIGHT) / 8); }  // nen (bit 1)

void RobotEyes::px(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    buf_[y * (WIDTH / 8) + (x >> 3)] &= (uint8_t)~(0x80 >> (x & 7));   // 0 = mat (sang)
}

void RobotEyes::hline(int x0, int y, int len) {
    for (int i = 0; i < len; i++) px(x0 + i, y);
}

void RobotEyes::box(int x0, int y0, int x1, int y1) {
    int l = std::min(x0, x1), r = std::max(x0, x1);
    int t = std::min(y0, y1), b = std::max(y0, y1);
    for (int y = t; y <= b; y++) hline(l, y, r - l + 1);
}

void RobotEyes::disc(int cx, int cy, int r) {
    if (r <= 0) return;
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        hline(cx - dx, cy + dy, 2 * dx + 1);
    }
}

void RobotEyes::roundRect(int x, int y, int w, int h, int r) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r <= 0) { box(x, y, x + w - 1, y + h - 1); return; }
    box(x + r, y, x + w - 1 - r, y + h - 1);
    box(x, y + r, x + w - 1, y + h - 1 - r);
    disc(x + r, y + r, r);
    disc(x + w - 1 - r, y + r, r);
    disc(x + r, y + h - 1 - r, r);
    disc(x + w - 1 - r, y + h - 1 - r, r);
}

void RobotEyes::tri(int x0, int y0, int x1, int y1, int x2, int y2) {
    int top = std::min(y0, std::min(y1, y2));
    int bot = std::max(y0, std::max(y1, y2));
    for (int y = top; y <= bot; y++) {
        float xs[3]; int n = 0;
        int px_[3] = {x0, x1, x2}, py_[3] = {y0, y1, y2};
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            int ya = py_[i], yb = py_[j];
            if ((y >= ya && y <= yb) || (y >= yb && y <= ya)) {
                if (ya == yb) { xs[n++] = (float)px_[i]; xs[n++] = (float)px_[j]; }
                else xs[n++] = px_[i] + (float)(px_[j] - px_[i]) * (y - ya) / (float)(yb - ya);
            }
        }
        if (n >= 2) {
            float lo = xs[0], hi = xs[0];
            for (int k = 1; k < n; k++) { lo = std::min(lo, xs[k]); hi = std::max(hi, xs[k]); }
            hline((int)lroundf(lo), y, (int)lroundf(hi - lo) + 1);
        }
    }
}

// small "z"/"Z" glyph drawn with lines
void RobotEyes::glyphZ(int x, int y, int s) {
    hline(x, y, s + 1);
    for (int i = 0; i <= s; i++) px(x + s - i, y + 1 + i);
    hline(x, y + s + 2, s + 1);
}

void RobotEyes::glyphTilde(int x, int y) {
    px(x + 1, y); px(x + 2, y);
    px(x, y + 1); px(x + 3, y + 1);
}

// Round/triangle cuts: set bit 1 (background colour) to carve the shape
static void cut_disc(uint8_t* buf, int cx, int cy, int r) {
    if (r <= 0) return;
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        for (int x = cx - dx; x <= cx + dx; x++) {
            if (x < 0 || x >= 128) continue;
            int y = cy + dy;
            if (y < 0 || y >= 64) continue;
            buf[y * 16 + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7));
        }
    }
}

static void cut_tri(uint8_t* buf, int x0, int y0, int x1, int y1, int x2, int y2) {
    int top = std::min(y0, std::min(y1, y2));
    int bot = std::max(y0, std::max(y1, y2));
    int px_[3] = {x0, x1, x2}, py_[3] = {y0, y1, y2};
    for (int y = top; y <= bot; y++) {
        float xs[4]; int n = 0;
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            int ya = py_[i], yb = py_[j];
            if ((y >= ya && y <= yb) || (y >= yb && y <= ya)) {
                if (ya == yb) { xs[n++] = (float)px_[i]; xs[n++] = (float)px_[j]; }
                else xs[n++] = px_[i] + (float)(px_[j] - px_[i]) * (y - ya) / (float)(yb - ya);
            }
        }
        if (n < 2) continue;
        float lo = xs[0], hi = xs[0];
        for (int k = 1; k < n; k++) { lo = std::min(lo, xs[k]); hi = std::max(hi, xs[k]); }
        for (int x = (int)lroundf(lo); x <= (int)lroundf(hi); x++) {
            if (x < 0 || x >= 128 || y < 0 || y >= 64) continue;
            buf[y * 16 + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7));
        }
    }
}

// ------------------------- frame loop -------------------------
void RobotEyes::Step() {
    const uint32_t now = now_ms();

    // demo: cycle moods on its own while idle (OliEyes demo mode)
    if (demo_ && idle_ && now - last_mood_switch_ > mood_switch_base_) {
        last_mood_switch_ = now;
        mood_switch_base_ = 3500 + rand_int(0, 2000);
        mood_ = (mood_ + 1) % MOOD_COUNT;
        ESP_LOGI(TAG, "demo mood -> %s", kMoodNames[mood_]);
    }

    UpdateLook(now);
    UpdateBlink(now);
    UpdateWinks(now);
    UpdateEasing();
    Render();
}

void RobotEyes::UpdateLook(uint32_t now) {
    if (double_take_) {
        uint32_t elapsed = now - double_take_start_;
        if (elapsed < 120) {
            look_tx_ = 16 * double_take_dir_;
            look_ty_ = 0;
            look_x_ = ease_to(look_x_, look_tx_, 0.6f);
            look_y_ = ease_to(look_y_, look_ty_, 0.6f);
        } else if (elapsed < 220) {
            blink_requested_ = true;
            look_tx_ = 0;
            look_x_ = ease_to(look_x_, 0, 0.5f);
            look_y_ = ease_to(look_y_, 0, 0.5f);
        } else if (elapsed < 500) {
            look_x_ = ease_to(look_x_, 0, 0.3f);
            look_y_ = ease_to(look_y_, 0, 0.3f);
        } else {
            double_take_ = false;
        }
        return;
    }

    if (mood_ == M_DIZZY) {   // mat xoay vong
        float angle = now / 260.0f;
        look_tx_ = cosf(angle) * 15;
        look_ty_ = sinf(angle) * 6;
        look_x_ = ease_to(look_x_, look_tx_, 0.25f);
        look_y_ = ease_to(look_y_, look_ty_, 0.25f);
        return;
    }

    if (idle_ && now - last_look_change_ > next_look_interval_) {
        last_look_change_ = now;
        next_look_interval_ = 3000 + rand_int(0, 4000);
        if (mood_ == M_SHY) {
            look_tx_ = rand_int(-14, -4);
            look_ty_ = rand_int(4, 10);
        } else if (rand_int(0, 99) < 70) {
            look_tx_ = rand_int(-16, 16);
            look_ty_ = rand_int(-6, 6);
        } else {
            look_tx_ = 0;
            look_ty_ = 0;
        }
    }
    look_x_ = ease_to(look_x_, idle_ ? look_tx_ : 0.0f, 0.08f);
    look_y_ = ease_to(look_y_, idle_ ? look_ty_ : 0.0f, 0.08f);

    if (mood_ == M_LAUGH) bounce_y_ = sinf(now / 65.0f) * 3.0f;
    else bounce_y_ = ease_to(bounce_y_, 0, 0.2f);
}

void RobotEyes::UpdateWinks(uint32_t now) {
    if (wink_right_phase_ == 1) {
        float t = (now - wink_right_start_) / 90.0f;
        wink_right_ = std::min(1.0f, t);
        if (t >= 1.0f) { wink_right_phase_ = 2; wink_right_start_ = now; }
    } else if (wink_right_phase_ == 2) {
        float t = (now - wink_right_start_) / 140.0f;
        wink_right_ = std::max(0.0f, 1.0f - t);
        if (t >= 1.0f) { wink_right_phase_ = 0; wink_right_ = 0; }
    }
    if (wink_left_phase_ == 1) {
        float t = (now - wink_left_start_) / 90.0f;
        wink_left_ = std::min(1.0f, t);
        if (t >= 1.0f) { wink_left_phase_ = 2; wink_left_start_ = now; }
    } else if (wink_left_phase_ == 2) {
        float t = (now - wink_left_start_) / 140.0f;
        wink_left_ = std::max(0.0f, 1.0f - t);
        if (t >= 1.0f) { wink_left_phase_ = 0; wink_left_ = 0; }
    }
}

void RobotEyes::UpdateBlink(uint32_t now) {
    int min_int = 2200, max_int = 5500;
    if (mood_ == M_SLEEPY || mood_ == M_BORED) { min_int = 1500; max_int = 3200; }
    if (mood_ == M_SURPRISED || mood_ == M_SCARED) { min_int = 4200; max_int = 8500; }
    if (mood_ == M_EXCITED) { min_int = 900; max_int = 2000; }

    if (blink_requested_ && blink_phase_ == 0) {
        blink_requested_ = false;
        blink_phase_ = 1;
        blink_phase_start_ = now;
    }
    if (blink_phase_ == 0 && idle_ && now - last_blink_ > next_blink_interval_) {
        if (rand_int(0, 99) < 18) {              // 18%: nhay 1 mat thay vi chop ca 2
            if (rand_int(0, 1)) TriggerWinkRight(now); else TriggerWinkLeft(now);
            last_blink_ = now;
            next_blink_interval_ = rand_int(min_int, max_int);
        } else {
            blink_phase_ = 1;
            blink_phase_start_ = now;
            last_blink_ = now;
            double_blink_armed_ = (rand_int(0, 99) < 18);   // 18%: chop them 1 cai nua
            next_blink_interval_ = rand_int(min_int, max_int);
        }
    }
    if (blink_phase_ == 1) {
        float t = (now - blink_phase_start_) / 90.0f;
        blink_ = std::min(1.0f, t);
        if (t >= 1.0f) { blink_phase_ = 2; blink_phase_start_ = now; }
    } else if (blink_phase_ == 2) {
        float t = (now - blink_phase_start_) / 140.0f;
        blink_ = std::max(0.0f, 1.0f - t);
        if (t >= 1.0f) {
            blink_phase_ = 0;
            blink_ = 0;
            if (double_blink_armed_) { double_blink_armed_ = false; blink_requested_ = true; }
        }
    }
}

void RobotEyes::UpdateEasing() {
    EyeTarget tl{}, tr{};
    GetMoodTarget(mood_, false, tl);
    GetMoodTarget(mood_, true, tr);
    left_.w  = ease_to(left_.w,  tl.w,  ease_speed_);
    left_.h  = ease_to(left_.h,  tl.h,  ease_speed_);
    left_.radius = ease_to(left_.radius, tl.radius, ease_speed_);
    left_.slantInner = ease_to(left_.slantInner, tl.slantInner, ease_speed_);
    left_.slantOuter = ease_to(left_.slantOuter, tl.slantOuter, ease_speed_);
    left_.squint = ease_to(left_.squint, tl.squint, ease_speed_);
    left_.yOff = ease_to(left_.yOff, tl.yOff, ease_speed_);

    right_.w = ease_to(right_.w, tr.w, ease_speed_);
    right_.h = ease_to(right_.h, tr.h, ease_speed_);
    right_.radius = ease_to(right_.radius, tr.radius, ease_speed_);
    right_.slantInner = ease_to(right_.slantInner, tr.slantInner, ease_speed_);
    right_.slantOuter = ease_to(right_.slantOuter, tr.slantOuter, ease_speed_);
    right_.squint = ease_to(right_.squint, tr.squint, ease_speed_);
    right_.yOff = ease_to(right_.yOff, tr.yOff, ease_speed_);
}

void RobotEyes::TriggerWinkRight(uint32_t now) {
    if (wink_right_phase_ == 0) { wink_right_phase_ = 1; wink_right_start_ = now; }
}
void RobotEyes::TriggerWinkLeft(uint32_t now) {
    if (wink_left_phase_ == 0) { wink_left_phase_ = 1; wink_left_start_ = now; }
}

// ------------------------- external control -------------------------
void RobotEyes::SetMood(const char* name) {
    if (!name) return;
    for (int i = 0; i < MOOD_COUNT; i++) {
        if (strcasecmp(name, kMoodNames[i]) == 0) {
            if (mood_ != i) ESP_LOGI(TAG, "doi mood -> %s", name);
            mood_ = i;
            last_mood_switch_ = now_ms();
            return;
        }
    }
    ESP_LOGW(TAG, "unknown mood: %s", name);
}

void RobotEyes::SetMoodForEmotion(const char* emotion) {
    if (!emotion) return;
    if (strcasecmp(emotion, "winking") == 0) { Wink(); return; }
    for (const auto& m : EMOTION_MAP) {
        if (strcasecmp(emotion, m.emotion) == 0) { SetMood(kMoodNames[m.mood]); return; }
    }
    SetMood("neutral");
}

void RobotEyes::Blink() { blink_requested_ = true; }
void RobotEyes::ForceRedraw() {
    // Poison the previous-frame cache: the next Step() always sees a difference and
    // redraws (used after panel wake/re-init, where the glass state is uncertain).
    if (pbuf_) memset(pbuf_, 0xA5, (size_t)(WIDTH * HEIGHT) / 8);
}
void RobotEyes::Wink(bool right_side) {
    uint32_t now = now_ms();
    if (right_side) TriggerWinkRight(now); else TriggerWinkLeft(now);
}
void RobotEyes::DoubleTake(bool right_first) {
    double_take_ = true;
    double_take_start_ = now_ms();
    double_take_dir_ = right_first ? 1 : -1;
}

// ------------------------- render -------------------------
void RobotEyes::Render() {
    if (!canvas_ || !buf_) return;
    if (lv_screen_active() != screen_) return;

    clear();

    // decorations: sleepy/bored -> "z Z" ; sick -> "~"
    if (mood_ == M_SLEEPY || mood_ == M_BORED) { glyphZ(WIDTH - 22, 2, 3); glyphZ(WIDTH - 13, 0, 5); }
    if (mood_ == M_SICK) glyphTilde(WIDTH - 18, HEIGHT - 10);

    int left_w  = (int)left_.w;
    int right_w = (int)right_.w;
    int base_left_x  = WIDTH / 2 - (EYE_GAP / 2 + left_w / 2);
    int base_right_x = WIDTH / 2 + (EYE_GAP / 2 + right_w / 2);
    int lx = base_left_x + (int)look_x_;
    int rx = base_right_x + (int)look_x_;
    int base_y = HEIGHT / 2;

    // left eye
    {
        int w = (int)left_.w;
        if (look_x_ < -6) w += (int)std::min((-look_x_ - 6) * 0.6f, 6.0f);
        int h = (int)left_.h, r = (int)left_.radius;
        int x = lx - w / 2;
        int y = base_y - h / 2 + (int)left_.yOff + (int)look_y_ + (int)bounce_y_;
        float blink = std::max(blink_, wink_left_);
        int hh = std::max(2, (int)(h * (1.0f - blink)));
        int yy = y + (h - hh);
        roundRect(x, yy, w, hh, r);
        if (left_.squint > 1) {
            float half_w = w / 2.0f;
            float sq = std::min(left_.squint, (float)(hh - 3));
            if (sq > 1) {
                float R = (sq * sq + half_w * half_w) / (2.0f * sq);
                cut_disc(buf_, lx, yy + hh + (int)(R - sq), (int)R);
            }
        }
        if (left_.slantInner > 1) {
            int cw = w / 2, ch = (int)left_.slantInner;
            cut_tri(buf_, x + w, yy, x + w - cw, yy, x + w, yy + ch);
        }
        if (left_.slantOuter > 1) {
            int cw = w / 2, ch = (int)left_.slantOuter;
            cut_tri(buf_, x, yy, x + cw, yy, x, yy + ch);
        }
    }
    // right eye
    {
        int w = (int)right_.w;
        if (look_x_ > 6) w += (int)std::min((look_x_ - 6) * 0.6f, 6.0f);
        int h = (int)right_.h, r = (int)right_.radius;
        int x = rx - w / 2;
        int y = base_y - h / 2 + (int)right_.yOff + (int)look_y_ + (int)bounce_y_;
        float blink = std::max(blink_, wink_right_);
        int hh = std::max(2, (int)(h * (1.0f - blink)));
        int yy = y + (h - hh);
        roundRect(x, yy, w, hh, r);
        if (right_.squint > 1) {
            float half_w = w / 2.0f;
            float sq = std::min(right_.squint, (float)(hh - 3));
            if (sq > 1) {
                float R = (sq * sq + half_w * half_w) / (2.0f * sq);
                cut_disc(buf_, rx, yy + hh + (int)(R - sq), (int)R);
            }
        }
        if (right_.slantInner > 1) {
            int cw = w / 2, ch = (int)right_.slantInner;
            cut_tri(buf_, x, yy, x + cw, yy, x, yy + ch);
        }
        if (right_.slantOuter > 1) {
            int cw = w / 2, ch = (int)right_.slantOuter;
            cut_tri(buf_, x + w, yy, x + w - cw, yy, x + w, yy + ch);
        }
    }

    // One-shot diagnostic: proves the eye shape really reached the canvas buffer.
    // clear() fills the background with bit 1, px()/disc()/roundRect() punch the eye
    // shape with bit 0, so the count of 1-bits is the background and the rest is the eye.
    static int diag_frames = 0;
    if (diag_frames < 3) {
        diag_frames++;
        int ones = 0;
        for (size_t i = 0; i < (size_t)(WIDTH * HEIGHT) / 8; i++) {
            uint8_t b = buf_[i];
            while (b) { ones += (b & 1); b >>= 1; }
        }
        int total = WIDTH * HEIGHT;
        ESP_LOGI(TAG, "diag frame %d: mood=%s look=(%.0f,%.0f) background bits=%d eye pixels=%d",
                 diag_frames, kMoodNames[mood_], look_x_, look_y_, ones, total - ones);
    }
    // Render-on-change: the 1-bit frame is only 1 KB, so compare it with the previous
    // frame first. Identical (eyes holding still between blinks) -> skip blit AND
    // invalidate entirely: zero LVGL work, zero I2C traffic, zero chance for this frame
    // to wedge the bus. A blink or glance changes pixels and flows through as usual.
    static constexpr size_t kFrameBytes = (size_t)(WIDTH * HEIGHT) / 8;
    if (pbuf_ != nullptr && memcmp(buf_, pbuf_, kFrameBytes) == 0) {
        return;
    }
    if (pbuf_ != nullptr) {
        memcpy(pbuf_, buf_, kFrameBytes);
    }
    blit();
    lv_obj_invalidate(canvas_);
}

// Expand the 1-bit scratch buffer into the RGB565 canvas. clear() fills the background
// with bit 1 and the drawing primitives punch the eye shape with bit 0 (see px()), so
// bit 1 -> background, bit 0 -> eye. The panel is colour-inverted (the chat UI proves it:
// it draws black text and the text is what lights up), therefore the eye - the part that
// must be lit - is LVGL black and the background LVGL white.
void RobotEyes::blit() {
    if (!rgb_ || !buf_) return;
    const int stride = WIDTH / 8;
    for (int y = 0; y < HEIGHT; y++) {
        const uint8_t* row = buf_ + (size_t)y * stride;
        uint16_t* out = rgb_ + (size_t)y * WIDTH;
        for (int xb = 0; xb < stride; xb++) {
            const uint8_t b = row[xb];
            for (int bit = 0; bit < 8; bit++) {
                const bool background = (b >> (7 - bit)) & 1;
                out[xb * 8 + bit] = background ? 0xFFFF : 0x0000;
            }
        }
    }
}
