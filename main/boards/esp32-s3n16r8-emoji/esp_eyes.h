#pragma once
// ---------------------------------------------------------------------------
// EspEyes - mat robot theo kieu "esp32-eyes" (Luis Llamas + Playful Technology,
// giay phep GPL/AGPL nhu thu muc main/boards/face). Port sang LVGL canvas (I1)
// cho man SH1106 128x64 don sac cua board esp32-s3n16r8-emoji.
// 25 kieu mat lay tu main/boards/face/EyePresets.h (xem esp_eyes_presets.inc).
// ---------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <lvgl.h>

struct EyeShape {
    int16_t OffsetX = 0;
    int16_t OffsetY = 0;
    int16_t Height = 40;
    int16_t Width = 40;
    float Slope_Top = 0;
    float Slope_Bottom = 0;
    int16_t Radius_Top = 8;
    int16_t Radius_Bottom = 8;
    // 4 truong nay thu vien goc khai bao nhung KHONG dung khi ve -> giu lai cho du
    // lieu preset trung khop, khong dua vao thuat toan.
    int16_t Inverse_Radius_Top = 0;
    int16_t Inverse_Radius_Bottom = 0;
    int16_t Inverse_Offset_Top = 0;
    int16_t Inverse_Offset_Bottom = 0;
};

class EspEyes {
public:
    void Initialize(lv_obj_t* screen);          // tao canvas 128x64 (I1) tren screen
    lv_obj_t* Screen() const { return screen_; }

    void SetPreset(const char* name, int duration_ms = 350);
    void SetPresetForEmotion(const char* emotion, int duration_ms = 350);
    void Blink(int duration_ms = 220);
    void Wink(bool left_side = true, int duration_ms = 260);
    void SetIdleLook(bool on) { idle_look_ = on; }

    // Goi ~30 lan/giay. PHAI duoc goi khi da giu lock LVGL (DisplayLockGuard).
    void Step();

    static std::string StyleNames();            // ten 25 kieu, ngan cach bang dau phay
    static bool IsValidStyle(const char* name);

private:
    void Render();
    void DrawEye(int cx, int cy, const EyeShape& s, float blink_amount);
    // nguyen thu ve (toa do man hinh, tu dong cat bien)
    void px(int x, int y);
    void hline(int x0, int y, int len);
    void box(int x0, int y0, int x1, int y1);
    void tri(int x0, int y0, int x1, int y1);
    void ellipseCorner(int corner, int x0, int y0, int rx, int ry);
    void clear();
    static void lerpShape(EyeShape& out, const EyeShape& a, const EyeShape& b, float t);

    static constexpr int WIDTH = 128;
    static constexpr int HEIGHT = 64;
    static constexpr int LEFT_CX = 39;          // tam mat trai  (giong emoji_controller)
    static constexpr int RIGHT_CX = 89;         // tam mat phai
    static constexpr int EYE_CY = 32;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    uint8_t* buf_ = nullptr;                    // 128*64/8 = 1024 byte

    EyeShape cur_{};                            // dang ve
    EyeShape from_{};
    EyeShape target_{};
    uint32_t morph_start_ = 0;
    uint32_t morph_end_ = 0;

    float blink_amt_ = 0;                       // 0..1
    uint32_t blink_start_ = 0;
    uint32_t blink_end_ = 0;
    bool blink_left_ = true;
    bool blink_right_ = true;

    float look_x_ = 0, look_y_ = 0;             // dich chuyen ca 2 mat
    float look_tx_ = 0, look_ty_ = 0;
    uint32_t next_look_ms_ = 0;
    uint32_t next_blink_ms_ = 0;
    bool idle_look_ = true;
    uint32_t frame_ = 0;
};
