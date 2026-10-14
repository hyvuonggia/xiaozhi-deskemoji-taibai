#pragma once

#include <libs/gif/lv_gif.h>

#include "display/lcd_display.h"

// Electron Bot expression GIF declarations - uses the same 6 expressions as Otto
LV_IMAGE_DECLARE(staticstate);  // Static state / neutral expression
LV_IMAGE_DECLARE(sad);          // Sad
LV_IMAGE_DECLARE(happy);        // Happy
LV_IMAGE_DECLARE(scare);        // Scared / surprised
LV_IMAGE_DECLARE(buxue);        // Confused
LV_IMAGE_DECLARE(anger);        // Angry

/**
 * @brief Electron Bot GIF expression display class
 * Inherits from LcdDisplay and adds GIF expression support
 */
class ElectronEmojiDisplay : public SpiLcdDisplay {
public:
    /**
     * @brief Constructor; parameters are the same as SpiLcdDisplay
     */
    ElectronEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, int offset_x, int offset_y, bool mirror_x,
                         bool mirror_y, bool swap_xy, DisplayFonts fonts);

    virtual ~ElectronEmojiDisplay() = default;

    // Override the emotion setting method
    virtual void SetEmotion(const char* emotion) override;

    // Override the chat message setting method
    virtual void SetChatMessage(const char* role, const char* content) override;

    // Override the icon setting method
    virtual void SetIcon(const char* icon) override;

private:
    void SetupGifContainer();

    lv_obj_t* emotion_gif_;  ///< GIF expression widget

    // Emotion mapping
    struct EmotionMap {
        const char* name;
        const lv_image_dsc_t* gif;
    };

    static const EmotionMap emotion_maps_[];
};