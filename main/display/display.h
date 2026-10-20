#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>
#include <chrono>

struct DisplayFonts {
    const lv_font_t* text_font = nullptr;
    const lv_font_t* icon_font = nullptr;
    const lv_font_t* emoji_font = nullptr;
};

class Display {
public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void SetIcon(const char* icon);
    virtual void SetPreviewImage(const lv_img_dsc_t* image);
    virtual void SetTheme(const std::string& theme_name);
    virtual std::string GetTheme() { return current_theme_name_; }
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void SetPowerSaveMode(bool on);

    inline int width() const { return width_; }
    inline int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;
    
    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t *display_ = nullptr;

    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;
    
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;
    std::string current_theme_name_;

    std::chrono::system_clock::time_point last_status_update_time_;
    esp_timer_handle_t notification_timer_ = nullptr;

    friend class DisplayLockGuard;
    friend class DisplayTryLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;
};


class DisplayLockGuard {
public:
    // NOTE: timeout is deliberately short (2 s). A display flush hung on a noisy bus
    // used to stall every holder for 30 s and freeze the whole device; with the task
    // watchdog panicking past 10 s, anything longer than that would reboot the board
    // over a stuck screen. Runtime paths should prefer DisplayTryLockGuard anyway.
    DisplayLockGuard(Display *display) : display_(display) {
        locked_ = display_->Lock(2000);
        if (!locked_) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        if (locked_) {
            display_->Unlock();
        }
    }

private:
    Display *display_;
    bool locked_ = false;
};

// Best-effort twin of DisplayLockGuard for runtime paths (state machine, eye
// renderer): tries the LVGL lock once and skips the update when it is held -
// typically by a display flush hung on a noisy I2C bus. Blocking on the lock here
// used to freeze the whole device (stuck LED, deaf to the wake word) over a stuck
// screen. Callers MUST check locked() and return early when false;
// unlike DisplayLockGuard this one never unlocks what it did not lock.
class DisplayTryLockGuard {
public:
    explicit DisplayTryLockGuard(Display *display) : display_(display) {
        locked_ = display_->Lock(0);
    }
    ~DisplayTryLockGuard() {
        if (locked_) {
            display_->Unlock();
        }
    }
    bool locked() const { return locked_; }

private:
    Display *display_;
    bool locked_ = false;
};

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override {
        return true;
    }
    virtual void Unlock() override {}
};

#endif
