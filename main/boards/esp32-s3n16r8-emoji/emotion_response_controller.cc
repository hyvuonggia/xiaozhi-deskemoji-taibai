/**
 * @file emotion_response_controller.cc
 * @brief Implementation file for the AI response emotion-response controller
 * 
 * Implements the AI response emotion-response controller, which automatically triggers emoji and servo actions based on the AI reply content
 */

#include "emotion_response_controller.h"
#include "emoji_controller.h"
#include "servo_controller.h"
#include "board.h"
#include "mcp_server.h"
#include "application.h"
#include "audio/audio_codec.h"

#include <esp_log.h>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cJSON.h>

#define TAG "EmotionController"

// Constructor
EmotionResponseController::EmotionResponseController(EmojiController* emoji_controller, ServoController* servo_controller, AudioCodec* audio_codec)
    : emoji_controller_(emoji_controller), 
      servo_controller_(servo_controller),
      audio_codec_(audio_codec),
      current_emotion_("neutral"),
      default_emotion_("neutral") {
}

// Destructor
EmotionResponseController::~EmotionResponseController() {
}

// Initialize the emotion-response controller
void EmotionResponseController::Initialize() {
    // Initialize the emotion-to-action mapping
    InitializeEmotionActions();
    
    // Initialize the emotion keyword mapping
    InitializeEmotionKeywords();
    
    ESP_LOGI(TAG, "EmotionResponseController initialized");
}

// Process an AI reply
void EmotionResponseController::ProcessAIResponse(const std::string& message) {
    // Add detailed logging
    ESP_LOGI(TAG, "Processing AI response: %s", message.c_str());
    
    // First check whether this is a volume control command
    if (ProcessVolumeCommand(message)) {
        // If it is a volume control command, it has already been handled; return immediately
        ESP_LOGI(TAG, "Recognized as a volume control command; processing complete");
        return;
    }
    
    // Check whether this is an expression action command
    if (ProcessEmotionCommand(message)) {
        // If it is an expression action command, it has already been handled; return immediately
        ESP_LOGI(TAG, "Recognized as an expression action command; processing complete");
        return;
    }
    
    // Analyze the emotion and action from the message content
    std::string emotion = "neutral";  // Default emotion is neutral
    std::string action = "";          // No specific action by default
    
    // Analyze the text content to obtain the emotion type
    emotion = AnalyzeText(message);
    
    // Determine from the message content whether a specific action should be performed
    if (ShouldNod(message)) {
        action = "nod";
        ESP_LOGI(TAG, "Content indicates agreement or affirmation; performing nod action");
    } else if (ShouldShake(message)) {
        action = "shake";
        ESP_LOGI(TAG, "Content indicates disagreement or refusal; performing head shake action");
    } else if (ShouldDance(message)) {
        action = "dance";
        ESP_LOGI(TAG, "Content indicates happiness or celebration; performing dance action");
    } else if (ShouldLookLeft(message)) {
        action = "look_left";
        ESP_LOGI(TAG, "Content mentions the left; looking left");
    } else if (ShouldLookRight(message)) {
        action = "look_right";
        ESP_LOGI(TAG, "Content mentions the right; looking right");
    } else if (ShouldLookUp(message)) {
        action = "look_up";
        ESP_LOGI(TAG, "Content mentions above; looking up");
    } else if (ShouldLookDown(message)) {
        action = "look_down";
        ESP_LOGI(TAG, "Content mentions below; looking down");
    }
    
    // Execute the emotion action
    ESP_LOGI(TAG, "Emotion inferred from text: %s", emotion.c_str());
    
    // If there is a specific action, execute the action first
    if (!action.empty()) {
        ExecuteEmotionAction(action);
    } else {
        // Otherwise execute the action corresponding to the emotion
        ExecuteEmotionAction(emotion);
    }
    
    ESP_LOGI(TAG, "Processed AI response, emotion: %s, action: %s", 
             emotion.c_str(), action.empty() ? "none" : action.c_str());
}

// Process an AI reply JSON
void EmotionResponseController::ProcessAIResponseJson(const cJSON* json_obj) {
    // Not implemented yet
}

// Process a volume control command
bool EmotionResponseController::ProcessVolumeCommand(const std::string& message) {
    // ESP_LOGI(TAG, "Processing volume control command: %s", message.c_str());
    
    // Get the AudioCodec instance
    AudioCodec* codec = audio_codec_;
    if (!codec) {
        codec = Board::GetInstance().GetAudioCodec();
        if (!codec) {
            ESP_LOGE(TAG, "Failed to get the AudioCodec instance");
            return false;
        }
    }
    
    // Convert to lowercase for processing
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // Match the various volume-set command patterns - use string search instead of regular expressions
    // Look for patterns such as "set volume to", "adjust volume to", "set volume as", etc.
    size_t pos = 0;
    int volume = -1;
    
    // Check the various volume-set command formats
    const char* volume_patterns[] = {
        "音量设为", "音量调为", "音量设置为", "音量调到", "声音设为", 
        "声音调为", "声音设置为", "声音调到", "把音量设为", "把音量调为", 
        "把音量设置为", "把音量调到", "将音量设为", "将音量调为", "将音量设置为", "将音量调到"
    };
    
    for (const char* pattern : volume_patterns) {
        pos = lower_text.find(pattern);
        if (pos != std::string::npos) {
            // Found a matching pattern; extract the number that follows
            pos += strlen(pattern);
            std::string volume_str;
            
            // Extract the numeric part
            while (pos < lower_text.length() && std::isdigit(lower_text[pos])) {
                volume_str += lower_text[pos];
                pos++;
            }
            
            if (!volume_str.empty()) {
                volume = std::stoi(volume_str);
                break;
            }
        }
    }
    
    if (volume >= 0) {
        // Clamp the volume to the 0-100 range
        volume = std::max(0, std::min(100, volume));
        
        // Set the volume
        codec->SetOutputVolume(volume);
        ESP_LOGI(TAG, "Setting volume to: %d", volume);
        
        // Trigger the happy expression
        ExecuteEmotionAction("happy");
        return true;
    }
    
    // Match the "increase volume" / "turn the volume up" patterns
    const char* volume_up_patterns[] = {
        "音量增加", "音量加大", "增加音量", "加大音量"
    };
    
    bool volume_up = false;
    for (const char* pattern : volume_up_patterns) {
        if (lower_text.find(pattern) != std::string::npos) {
            volume_up = true;
            break;
        }
    }
    
    if (volume_up) {
        // Get the current volume and increase it by 10
        int volume = codec->output_volume() + 10;
        volume = std::min(100, volume);
        
        // Set the new volume
        codec->SetOutputVolume(volume);
        ESP_LOGI(TAG, "Increasing volume to: %d", volume);
        
        // Trigger the happy expression
        ExecuteEmotionAction("happy");
        return true;
    }
    
    // Match the "decrease volume" / "turn the volume down" patterns
    const char* volume_down_patterns[] = {
        "音量减小", "音量降低", "减小音量", "降低音量"
    };
    
    bool volume_down = false;
    for (const char* pattern : volume_down_patterns) {
        if (lower_text.find(pattern) != std::string::npos) {
            volume_down = true;
            break;
        }
    }
    
    if (volume_down) {
        // Get the current volume and decrease it by 10
        int volume = codec->output_volume() - 10;
        volume = std::max(0, volume);
        
        // Set the new volume
        codec->SetOutputVolume(volume);
        ESP_LOGI(TAG, "Decreasing volume to: %d", volume);
        
        // Trigger the happy expression
        ExecuteEmotionAction("happy");
        return true;
    }
    
    // Match the "mute" / "turn off the sound" patterns
    const char* mute_patterns[] = {
        "静音", "关闭声音", "声音关闭"
    };
    
    bool mute = false;
    for (const char* pattern : mute_patterns) {
        if (lower_text.find(pattern) != std::string::npos) {
            mute = true;
            break;
        }
    }
    
    if (mute) {
        // Set the volume to 0
        codec->SetOutputVolume(0);
        ESP_LOGI(TAG, "Muted");
        
        // Trigger the happy expression
        ExecuteEmotionAction("happy");
        return true;
    }
    
    // Match the "maximum volume" / "volume max" patterns
    const char* max_volume_patterns[] = {
        "最大音量", "音量最大"
    };
    
    bool max_volume = false;
    for (const char* pattern : max_volume_patterns) {
        if (lower_text.find(pattern) != std::string::npos) {
            max_volume = true;
            break;
        }
    }
    
    if (max_volume) {
        // Set the volume to 100
        codec->SetOutputVolume(100);
        ESP_LOGI(TAG, "Setting volume to maximum");
        
        // Trigger the happy expression
        ExecuteEmotionAction("happy");
        return true;
    }
    
    // No volume control command matched
    return false;
}

// Process an Alert message
void EmotionResponseController::ProcessAlert(const char* status, const char* message, const char* emotion) {
    // If an explicit emotion tag is provided, use it directly
    if (emotion && strlen(emotion) > 0) {
        ExecuteEmotionAction(emotion);
        ESP_LOGI(TAG, "Processed Alert with emotion: %s", emotion);
        return;
    }
    
    // Otherwise analyze the status and message content
    std::string combined_text = std::string(status) + " " + std::string(message);
    std::string detected_emotion = AnalyzeText(combined_text);
    
    // Execute the emotion action
    ExecuteEmotionAction(detected_emotion);
    
    ESP_LOGI(TAG, "Processed Alert, detected emotion: %s", detected_emotion.c_str());
}

// Register emotion keywords
void EmotionResponseController::RegisterEmotionKeywords(const std::string& emotion, const std::vector<std::string>& keywords) {
    emotion_keywords_[emotion] = keywords;
}

// Set the default emotion
void EmotionResponseController::SetDefaultEmotion(const std::string& emotion) {
    default_emotion_ = emotion;
}

// Manually trigger an emotion response
void EmotionResponseController::TriggerEmotion(const std::string& emotion) {
    ESP_LOGI(TAG, "Triggered emotion: %s", emotion.c_str());
    current_emotion_ = emotion;
    
    // Use the emotion-to-action mapping
    ESP_LOGI(TAG, "Performing emotion action: %s", emotion.c_str());
    ExecuteEmotionAction(emotion);
}

// Get the current emotion
const std::string& EmotionResponseController::GetCurrentEmotion() const {
    return current_emotion_;
}

// Initialize the emotion-to-action mapping
void EmotionResponseController::InitializeEmotionActions() {
    // Define the action for each emotion
    
    // Happy-series emotion actions
    emotion_actions_["happy"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::HAPPY);
    };
    
    emotion_actions_["laughing"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::LAUGHING);
    };
    
    emotion_actions_["funny"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::FUNNY);
    };
    
    // Sad-series emotion actions
    emotion_actions_["sad"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::SAD);
    };
    
    emotion_actions_["cry"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::CRY);
    };
    
    // Anger emotion action
    emotion_actions_["anger"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::ANGER);
    };
    
    // Surprise-series emotion actions
    emotion_actions_["surprise"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::SURPRISE);
    };
    
    emotion_actions_["shocked"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::SHOCKED);
    };
    
    // Confused and thinking emotion actions
    emotion_actions_["confused"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::CONFUSED);
    };
    
    emotion_actions_["thinking"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::THINKING);
    };
    
    // Neutral emotion action
    emotion_actions_["neutral"] = [this]() {
        // Show the neutral expression
        emoji_controller_->EyeCenter();
        emoji_controller_->EyeBlink();
    };
    
    // Sleep emotion action
    emotion_actions_["sleep"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::SLEEP);
    };
    
    // Wake-up emotion action
    emotion_actions_["wakeup"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::WAKEUP);
    };
    
    // Look-left emotion action
    emotion_actions_["look_left"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::LOOK_LEFT);
        if (servo_controller_) servo_controller_->HeadLeft(20);   // the point of the command is the head, not the eyes
    };
    
    // Look-right emotion action
    emotion_actions_["look_right"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::LOOK_RIGHT);
        if (servo_controller_) servo_controller_->HeadRight(20);
    };
    
    // Nod emotion action
    emotion_actions_["nod"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::HEAD_NOD);
        if (servo_controller_) servo_controller_->HeadNod(15);
    };
    
    // Head-shake emotion action
    emotion_actions_["shake"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::HEAD_SHAKE);
        if (servo_controller_) servo_controller_->HeadShake(10);
    };
    
    // Look-up emotion action
    emotion_actions_["look_up"] = [this]() {
        emoji_controller_->EyeUp();
        servo_controller_->HeadUp();
    };
    
    // Look-down emotion action
    emotion_actions_["look_down"] = [this]() {
        emoji_controller_->EyeDown();
        servo_controller_->HeadDown();
    };
    
    // Center emotion action
    emotion_actions_["look_center"] = [this]() {
        emoji_controller_->EyeCenter();
        servo_controller_->HeadCenter();
    };
    
    // Nod-and-happy emotion action
    emotion_actions_["nod_happy"] = [this]() {
        emoji_controller_->EyeHappy();
        servo_controller_->HeadNod(15);
    };
    
    // Head-shake-and-angry emotion action
    emotion_actions_["shake_angry"] = [this]() {
        emoji_controller_->EyeAnger();
        servo_controller_->HeadShake(10);
    };
    
    // Spin-round-and-round emotion action
    emotion_actions_["spin"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::HEAD_ROLL);
    };
    
    // Blink emotion action
    emotion_actions_["blink"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::BLINK, 12);
    };
    
    // Dance emotion action
    emotion_actions_["dance"] = [this]() {
        // Show the happy expression
        emoji_controller_->EyeHappy();
        
        // Perform the dance action: rhythmic nodding plus spinning
        // Nod a few times first
        servo_controller_->HeadNod(15);
        servo_controller_->HeadRoll();
        
        // Return to the center position
        servo_controller_->HeadCenter(10);
    };
    
    // Awkward emotion action
    emotion_actions_["awkward"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::AWKWARD);
    };
    
    // Heart/loving expression action
    emotion_actions_["loving"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::LOVING);
    };
    
    // Kiss expression action
    emotion_actions_["kissy"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::KISSY);
    };
    
    // Cool expression action
    emotion_actions_["cool"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::COOL);
    };
    
    // Confident expression action
    emotion_actions_["confident"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::CONFIDENT);
    };
    
    // Relaxed expression action
    emotion_actions_["relaxed"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::RELAXED);
    };
    
    // Delicious expression action
    emotion_actions_["delicious"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::DELICIOUS);
    };
    
    // Silly (strange) expression action
    emotion_actions_["silly"] = [this]() {
        emoji_controller_->PlayAnimation(AnimationType::SILLY);
    };
}

void EmotionResponseController::InitializeEmotionKeywords() {
    // Clear the existing mapping
    emotion_keywords_.clear();
    
    // Sleep emotion keywords (the Xiaozhi framework has "sleepy", but its behavior may differ)
    std::vector<std::string> sleep_keywords = {
        "睡觉", "睡眠", "睡了", "困了", "和平呢", "睡觉呢", "睡觉了", 
        "晚安", "安慰", "晕", "晕晕的", "好累", "累了", "打哺欠", "好困",
        "sleep", "sleeping", "sleepy", "tired", "exhausted", "rest", "nap", "goodnight"
    };
    RegisterEmotionKeywords("sleep", sleep_keywords);
    
    // Wake-up emotion keywords
    std::vector<std::string> wakeup_keywords = {
        "醒醒", "醒醒呀", "醒醒了", "醒来", "起床", "醒醒眼", "醒神",
        "早安", "红斗呀", "新的一天", "发生", "来吧", "闲着了",
        "wake", "wakeup", "awake", "awaken", "rise", "arise", "morning", "hello"
    };
    RegisterEmotionKeywords("wakeup", wakeup_keywords);
    
    ESP_LOGI(TAG, "Emotion keywords initialized");
}

// Analyze the text content
std::string EmotionResponseController::AnalyzeText(const std::string& text) {
    // Convert to lowercase for matching
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // Iterate over the emotion keyword mapping to find a matching keyword
    for (const auto& emotion_pair : emotion_keywords_) {
        const std::string& emotion = emotion_pair.first;
        const std::vector<std::string>& keywords = emotion_pair.second;
        
        for (const std::string& keyword : keywords) {
            // Use a simple string search instead of regular expressions to reduce stack usage
            if (lower_text.find(keyword) != std::string::npos) {
                return emotion;
            }
        }
    }
    
    // If no keyword matched, return the default emotion
    return default_emotion_;
}

// Execute an emotion action
void EmotionResponseController::ExecuteEmotionAction(const std::string& emotion) {
    ESP_LOGI(TAG, "Performing emotion action: %s", emotion.c_str());
    
    // Update the current emotion
    current_emotion_ = emotion;
    
    // Look up and execute the emotion action
    auto it = emotion_actions_.find(emotion);
    if (it != emotion_actions_.end()) {
        // If a matching emotion action is found, execute it
        it->second();
    } else {
        // If no matching emotion action is found, use the default expression
        if (emoji_controller_ != nullptr) {
            emoji_controller_->PlayAnimation(AnimationType::BLINK, 10);
        }
    }
}

// Process an expression action command
bool EmotionResponseController::ProcessEmotionCommand(const std::string& message) {
    // Convert to lowercase for matching
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    ESP_LOGI(TAG, "Checking for an expression action command: %s", message.c_str());
    
    // Check whether it contains an emotion recognized by the Xiaozhi framework
    // Emotions recognized by the Xiaozhi framework usually appear in a specific format, e.g. "[emotion:happy]"
    const std::string emotion_prefix = "[emotion:";
    const std::string emotion_suffix = "]";
    
    size_t prefix_pos = lower_text.find(emotion_prefix);
    if (prefix_pos != std::string::npos) {
        size_t start_pos = prefix_pos + emotion_prefix.length();
        size_t end_pos = lower_text.find(emotion_suffix, start_pos);
        
        if (end_pos != std::string::npos) {
            // Extract the emotion recognized by the Xiaozhi framework
            std::string recognized_emotion = lower_text.substr(start_pos, end_pos - start_pos);
            ESP_LOGI(TAG, "Detected emotion recognized by the Xiaozhi framework: %s", recognized_emotion.c_str());
            
            // Use the existing emotion-to-action mapping directly
            ESP_LOGI(TAG, "Using the existing emotion-to-action mapping: %s", recognized_emotion.c_str());
            ExecuteEmotionAction(recognized_emotion);
            return true;
        }
    }
    
    // Keep only action-command keyword detection; remove the emotion keyword detection that duplicates the Xiaozhi framework
    
    // Look-left command
    const char* look_left_keywords[] = {"看向左边", "向左看", "左转", "往左看", "左看", "看左边"};
    for (const char* keyword : look_left_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected look-left command");
            ExecuteEmotionAction("look_left");
            return true;
        }
    }
    
    // Look-right command
    const char* look_right_keywords[] = {"看向右边", "向右看", "右转", "往右看", "右看", "看右边"};
    for (const char* keyword : look_right_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected look-right command");
            ExecuteEmotionAction("look_right");
            return true;
        }
    }
    
    // Look-up command
    const char* look_up_keywords[] = {"抬头", "向上看", "看上面", "抬头看"};
    for (const char* keyword : look_up_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected look-up command");
            ExecuteEmotionAction("look_up");
            return true;
        }
    }
    
    // Look-down command
    const char* look_down_keywords[] = {"低头", "向下看", "看下面", "低头看"};
    for (const char* keyword : look_down_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected look-down command");
            ExecuteEmotionAction("look_down");
            return true;
        }
    }
    
    // Center command
    const char* look_center_keywords[] = {"居中", "回正", "恢复正常", "回到中心"};
    for (const char* keyword : look_center_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected center command");
            ExecuteEmotionAction("look_center");
            return true;
        }
    }
    
    // Nod command
    const char* nod_keywords[] = {"点头", "点下头", "说是", "表示同意", "说是的"};
    for (const char* keyword : nod_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected nod command");
            ExecuteEmotionAction("nod");
            return true;
        }
    }
    
    // Head-shake command
    const char* shake_keywords[] = {"摇头", "摇下头", "说不是", "表示否定", "说不是的"};
    for (const char* keyword : shake_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected head-shake command");
            ExecuteEmotionAction("shake");
            return true;
        }
    }
    
    // Spin command
    const char* spin_keywords[] = {"转圈", "圈圈", "绕圈", "转个圈", "转一圈"};
    for (const char* keyword : spin_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected spin command");
            ExecuteEmotionAction("spin");
            return true;
        }
    }
    
    // Dance command
    const char* dance_keywords[] = {"跳舞", "舞蹈", "跳个舞", "来支舞", "跳一段"};
    for (const char* keyword : dance_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected dance command");
            ExecuteEmotionAction("dance");
            return true;
        }
    }
    
    // Blink command
    const char* blink_keywords[] = {"眨眼", "眨一下", "眨一眨", "眨", "眨眼了"};
    for (const char* keyword : blink_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected blink command");
            ExecuteEmotionAction("blink");
            return true;
        }
    }
    
    // If no expression action command matched, return false
    return false;
}

// Determine whether a nod is appropriate (agreement, affirmation)
bool EmotionResponseController::ShouldNod(const std::string& message) {
    // Convert to lowercase for matching
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // First check for a negative word; if present, a nod is not appropriate
    const char* negative_keywords[] = {
        "不", "否", "没", "无", "别", "莫", "勿", "非", "未"
    };
    
    for (const char* keyword : negative_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            // Contains a negative word; a nod is not appropriate
            ESP_LOGI(TAG, "Detected negative word '%s'; not performing nod action", keyword);
            return false;
        }
    }
    
    // Keywords indicating agreement or affirmation
    const char* nod_keywords[] = {
         "是的！", "对的！","对的。", "正确！", "同意！", "理解！", "明白！", "懂！", "知道！", "没问题！",
         "嗯嗯","赞成！", "支持！", "认同！",  "点头", "点个头", "点了个头", "nods", "nod"
    };
    
    for (const char* keyword : nod_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected affirmative word '%s'; performing nod action", keyword);
            return true;
        }
    }
    
    return false;
}

// Determine whether a head shake is appropriate (negation, refusal)
bool EmotionResponseController::ShouldShake(const std::string& message) {
    // Convert to lowercase for matching
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // Keywords indicating negation or refusal
    const char* shake_keywords[] = {
        "不是！", "不是。", "不对！", "不对。", "不行！", "不行。", "不可以", "不能", "不要！","不要。",
        "不同意！", "不同意。","拒绝！",  "不接受！",
        "不好！", "不好。", "不正确", "不准确", "不允许", "不可能！", "不可能。""没有。", "不存在", "摇头", "摇个头", "摇了个头"
    };
    
    // First check the complete negative phrases
    for (const char* keyword : shake_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            ESP_LOGI(TAG, "Detected negative phrase '%s'; performing head-shake action", keyword);
            return true;
        }
    }
    
    // Then check standalone negative words (ensure the message consists only of the negative word or starts with it)
    const char* simple_negatives[] = {
        "不", "否", "没"
    };
    
    for (const char* keyword : simple_negatives) {
        // If the message is exactly this word, or starts with it followed by a space or punctuation
        if (lower_text == keyword) {
            ESP_LOGI(TAG, "Detected standalone negative word '%s'; performing head-shake action", keyword);
            return true;
        }
        
        // Check whether it starts with this keyword
        if (lower_text.find(keyword) == 0 && lower_text.length() > strlen(keyword)) {
            // Get the first character after the keyword
            char next_char = lower_text[strlen(keyword)];
            // Check whether it is a space or an English punctuation mark
            if (next_char == ' ' || next_char == '.' || next_char == ',') {
                ESP_LOGI(TAG, "Detected standalone negative word '%s'; performing head-shake action", keyword);
                return true;
            }
        }
    }
    
    return false;
}

// Determine whether dancing is appropriate (happiness, celebration)
bool EmotionResponseController::ShouldDance(const std::string& message) {
    // Convert to lowercase for matching
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // Keywords indicating happiness or celebration
    const char* dance_keywords[] = {
        "跳舞", "舞蹈", "跳", "舞", "动感", "节奏", "音乐", "律动", "跳个舞",
        "dance", "dancing", "jump", "move", "groove", "rhythm", "music", "beat"
    };
    
    for (const char* keyword : dance_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

// Determine whether looking left is appropriate
bool EmotionResponseController::ShouldLookLeft(const std::string& message) {
    // Convert to lowercase for matching
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // Keywords mentioning the left
    const char* look_left_keywords[] = {
        "左边", "左侧", "左方", "向左", "往左", "左转", "左看", "看左边"
    };
    
    for (const char* keyword : look_left_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

// Determine whether looking right is appropriate
bool EmotionResponseController::ShouldLookRight(const std::string& message) {
    // Convert to lowercase for matching
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // Keywords mentioning the right
    const char* look_right_keywords[] = {
        "右边", "右侧", "右方", "向右", "往右", "右转", "右看", "看右边"
    };
    
    for (const char* keyword : look_right_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

// Determine whether looking up is appropriate
bool EmotionResponseController::ShouldLookUp(const std::string& message) {
    // Convert to lowercase for matching
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // Keywords mentioning above
    const char* look_up_keywords[] = {
        "上面", "上方", "上边", "向上", "往上", "抬头", "仰头", "看上面", "看天空", "天上"
    };
    
    for (const char* keyword : look_up_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

// Determine whether looking down is appropriate
bool EmotionResponseController::ShouldLookDown(const std::string& message) {
    // Convert to lowercase for matching
    std::string lower_text = message;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    // Keywords mentioning below
    const char* look_down_keywords[] = {
        "下面", "下方", "下边", "向下", "往下", "低头", "俯首", "看下面", "看地面", "地上"
    };
    
    for (const char* keyword : look_down_keywords) {
        if (lower_text.find(keyword) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

// Emotion IoT interface implementation
namespace iot {
    // Global emotion-response controller pointer, used by the default constructor
    static EmotionResponseController* g_emotion_controller = nullptr;
    
    // Set the global emotion-response controller pointer
    void SetGlobalEmotionController(EmotionResponseController* controller) {
        g_emotion_controller = controller;
    }
    
    // Get the global emotion-response controller pointer
    EmotionResponseController* GetGlobalEmotionController() {
        return g_emotion_controller;
    }
} // namespace iot
