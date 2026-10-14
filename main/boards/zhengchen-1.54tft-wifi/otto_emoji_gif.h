#pragma once

#include <lvgl.h>
#include <esp_log.h>
#include <cstring>

// Declare expression GIFs
LV_IMAGE_DECLARE(staticstate);  // Static state / neutral expression
LV_IMAGE_DECLARE(sad);          // Sad
LV_IMAGE_DECLARE(happy);        // Happy
LV_IMAGE_DECLARE(scare);        // Startled / surprised
LV_IMAGE_DECLARE(buxue);        // Refuse-to-learn / confused
LV_IMAGE_DECLARE(anger);        // Angry

/**
 * @brief Get the corresponding GIF image resource by expression name
 * 
 * @param name Expression name
 * @return const lv_img_dsc_t* GIF image resource, or nullptr if it does not exist
 */
inline const lv_img_dsc_t* otto_emoji_gif_get_by_name(const char* name) {
    if (!name) return nullptr;

    // Neutral / calm expressions
    if (strcmp(name, "neutral") == 0 || 
        strcmp(name, "relaxed") == 0 || 
        strcmp(name, "sleepy") == 0) {
        return &staticstate;
    }
    
    // Happy expressions
    if (strcmp(name, "happy") == 0 || 
        strcmp(name, "laughing") == 0 || 
        strcmp(name, "funny") == 0 ||
        strcmp(name, "loving") == 0 ||
        strcmp(name, "confident") == 0 ||
        strcmp(name, "winking") == 0 ||
        strcmp(name, "cool") == 0 ||
        strcmp(name, "delicious") == 0 ||
        strcmp(name, "kissy") == 0 ||
        strcmp(name, "silly") == 0) {
        return &happy;
    }
    
    // Sad expressions
    if (strcmp(name, "sad") == 0 || 
        strcmp(name, "crying") == 0) {
        return &sad;
    }
    
    // Angry expressions
    if (strcmp(name, "angry") == 0) {
        return &anger;
    }
    
    // Surprised expressions
    if (strcmp(name, "surprised") == 0 || 
        strcmp(name, "shocked") == 0) {
        return &scare;
    }
    
    // Thinking / confused expressions
    if (strcmp(name, "thinking") == 0 || 
        strcmp(name, "confused") == 0 || 
        strcmp(name, "embarrassed") == 0) {
        return &buxue;
    }
    
    // Default to a neutral expression
    return &staticstate;
} 