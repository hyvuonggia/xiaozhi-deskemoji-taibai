#pragma once
// ---------------------------------------------------------------------------
// RobotEyes - OliEyes-style eye engine (Oliray-ka/OliEyes, MIT licence).
// Ported from examples/SH1106_i12C.ino to ESP-IDF/LVGL (I1 canvas) for the
// 128x64 SH1106 display of the esp32-s3n16r8-emoji board.
//   - 20 mood (neutral, happy, sad, angry, surprised, sleepy, love, suspicious,
//     confused, scared, laugh, bored, excited, shy, annoyed, dizzy, sick,
//     proud, frustrated, curious)
//   - smooth easing morphs, mood-dependent random blinks, single-eye winks,
//     double-take, idle looking around, bouncing while laughing, "z Z" when
//     sleepy/bored, "~" when sick, rolling eyes when dizzy, optional demo cycling.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <lvgl.h>

class RobotEyes {
public:
    void Initialize(lv_obj_t* screen);
    lv_obj_t* Screen() const { return screen_; }

    // --- control API ---
    void SetMood(const char* name);                 // ten trong 20 mood
    void SetMoodForEmotion(const char* emotion);    // emotion cua server -> mood
    void Blink();
    void Wink(bool right_side = true);
    void DoubleTake(bool right_first = true);
    void SetDemo(bool on) { demo_ = on; }
    // While in conversation: turn off idle behaviours (demo cycling + random looks)
    void SetIdleBehaviors(bool on) { idle_ = on; }
    void SetIdleGlances(bool on) { glances_ = on; }  // random look-arounds (cosmetic)
    void SetEaseSpeed(float s) { ease_speed_ = (s < 0.01f ? 0.01f : (s > 1.0f ? 1.0f : s)); }
    void ForceRedraw();   // forget the previous frame so the next render always hits the bus

    // Call ~40 times per second. MUST be called with the LVGL lock held (DisplayLockGuard).
    void Step();

    static std::string StyleNames();
    static bool IsValidStyle(const char* name);

private:
    struct EyeTarget {
        float w, h, radius, slantInner, slantOuter, squint, yOff;
    };
    typedef EyeTarget EyeState;

    void GetMoodTarget(int mood, bool is_right, EyeTarget& t) const;
    void Render();
    void UpdateLook(uint32_t now);
    void UpdateBlink(uint32_t now);
    void UpdateWinks(uint32_t now);
    void UpdateEasing();
    void TriggerWinkRight(uint32_t now);
    void TriggerWinkLeft(uint32_t now);
    // drawing primitives
    void px(int x, int y);
    void hline(int x0, int y, int len);
    void box(int x0, int y0, int x1, int y1);
    void roundRect(int x, int y, int w, int h, int r);
    void disc(int cx, int cy, int r);
    void tri(int x0, int y0, int x1, int y1, int x2, int y2);
    void glyphZ(int x, int y, int size);
    void glyphTilde(int x, int y);
    void clear();
    void blit();       // expand the 1-bit scratch buffer into the RGB565 canvas

    static constexpr int WIDTH = 128;
    static constexpr int HEIGHT = 64;
    static constexpr int EYE_GAP = 16;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    uint8_t* buf_ = nullptr;      // 1-bit scratch buffer the drawing primitives write into
    uint8_t* pbuf_ = nullptr;     // previous 1-bit frame: if identical, skip blit+invalidate (zero I2C)
    uint16_t* rgb_ = nullptr;     // RGB565 canvas buffer (what LVGL actually renders)

    int mood_ = 0;                  // 0 = neutral
    EyeState left_{}, right_{};
    float ease_speed_ = 0.12f;

    float look_x_ = 0, look_y_ = 0;
    float look_tx_ = 0, look_ty_ = 0;
    uint32_t last_look_change_ = 0;
    uint32_t next_look_interval_ = 2000;

    bool double_take_ = false;
    uint32_t double_take_start_ = 0;
    int double_take_dir_ = 1;

    float blink_ = 0;
    int blink_phase_ = 0;
    uint32_t last_blink_ = 0;
    uint32_t next_blink_interval_ = 3000;
    uint32_t blink_phase_start_ = 0;
    bool blink_requested_ = false;
    bool double_blink_armed_ = false;

    float wink_right_ = 0, wink_left_ = 0;
    int wink_right_phase_ = 0, wink_left_phase_ = 0;
    uint32_t wink_right_start_ = 0, wink_left_start_ = 0;

    float bounce_y_ = 0;

    bool demo_ = false;   // off by default: the board drives moods by context; enable by voice ("change your face by itself")
    bool idle_ = true;
    bool glances_ = false;  // look-arounds OFF: status-first face, blink only
    uint32_t last_mood_switch_ = 0;
    uint32_t mood_switch_base_ = 3500;
    uint32_t last_step_ms_ = 0;
};
