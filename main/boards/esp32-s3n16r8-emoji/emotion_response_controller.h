/**
 * @file emotion_response_controller.h
 * @brief Header file for the AI response emotion-response controller
 * 
 * Defines the interface of the AI response emotion-response controller, which automatically triggers emoji and servo actions based on the AI reply content
 */

#ifndef EMOTION_RESPONSE_CONTROLLER_H
#define EMOTION_RESPONSE_CONTROLLER_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <unordered_map>
#include "emoji_controller.h"
#include "servo_controller.h"
#include "audio_codec.h"
#include "mcp_server.h"

// Forward declarations
class EmotionResponseController;
class EmojiController;
class ServoController;
class AudioCodec;

/**
 * @class EmotionResponseController
 * @brief Emotion-response controller class that analyzes the AI reply content and triggers the corresponding emoji and servo actions
 */
class EmotionResponseController {
public:
    /**
     * @brief Constructor
     * @param emoji_controller Pointer to the emoji controller
     * @param servo_controller Pointer to the servo controller
     * @param audio_codec Pointer to the audio codec
     */
    EmotionResponseController(EmojiController* emoji_controller, ServoController* servo_controller, AudioCodec* audio_codec = nullptr);
    
    /**
     * @brief Destructor
     */
    ~EmotionResponseController();
    
    /**
     * @brief Initialize the emotion-response controller
     */
    void Initialize();
    
    /**
     * @brief Process an AI reply
     * @param message The AI reply message
     */
    void ProcessAIResponse(const std::string& message);
    
    /**
     * @brief Process an AI reply JSON
     * @param json_obj The AI reply JSON object
     */
    void ProcessAIResponseJson(const cJSON* json_obj);
    
    /**
     * @brief Process an Alert message
     * @param status The status message
     * @param message The detailed message
     * @param emotion The emotion tag
     */
    void ProcessAlert(const char* status, const char* message, const char* emotion = "");
    
    /**
     * @brief Register emotion keywords
     * @param emotion The emotion type
     * @param keywords The keyword list
     */
    void RegisterEmotionKeywords(const std::string& emotion, const std::vector<std::string>& keywords);
    
    /**
     * @brief Set the default emotion
     * @param emotion The default emotion type
     */
    void SetDefaultEmotion(const std::string& emotion);
    
    /**
     * @brief Manually trigger an emotion response
     * @param emotion The emotion type
     */
    void TriggerEmotion(const std::string& emotion);
    
    /**
     * @brief Get the current emotion
     * @return The current emotion type
     */
    const std::string& GetCurrentEmotion() const;
    
    /**
     * @brief Process a volume control command
     * @param message The AI reply message
     * @return true if it is a volume control command, otherwise false
     */
    bool ProcessVolumeCommand(const std::string& message);
    
    /**
     * @brief Process an expression action command
     * @param message The AI reply message
     * @return true if it is an expression action command, otherwise false
     */
    bool ProcessEmotionCommand(const std::string& message);
    

    
    /**
     * @brief Determine whether the message content should trigger a nod (agreement, affirmation)
     * @param message The message content
     * @return true if a nod is appropriate, otherwise false
     */
    bool ShouldNod(const std::string& message);
    
    /**
     * @brief Determine whether the message content should trigger a head shake (negation, refusal)
     * @param message The message content
     * @return true if a head shake is appropriate, otherwise false
     */
    bool ShouldShake(const std::string& message);
    
    /**
     * @brief Determine whether the message content should trigger a dance (happiness, celebration)
     * @param message The message content
     * @return true if dancing is appropriate, otherwise false
     */
    bool ShouldDance(const std::string& message);
    
    /**
     * @brief Determine whether the message content should trigger a look-left action
     * @param message The message content
     * @return true if looking left is appropriate, otherwise false
     */
    bool ShouldLookLeft(const std::string& message);
    
    /**
     * @brief Determine whether the message content should trigger a look-right action
     * @param message The message content
     * @return true if looking right is appropriate, otherwise false
     */
    bool ShouldLookRight(const std::string& message);
    
    /**
     * @brief Determine whether the message content should trigger a look-up action
     * @param message The message content
     * @return true if looking up is appropriate, otherwise false
     */
    bool ShouldLookUp(const std::string& message);
    
    /**
     * @brief Determine whether the message content should trigger a look-down action
     * @param message The message content
     * @return true if looking down is appropriate, otherwise false
     */
    bool ShouldLookDown(const std::string& message);
    
private:
    EmojiController* emoji_controller_;  // Emoji controller
    ServoController* servo_controller_;  // Servo controller
    AudioCodec* audio_codec_;  // Audio codec
    std::string current_emotion_;        // Current emotion
    std::string default_emotion_;        // Default emotion
    
    // Emotion keyword mapping table
    std::unordered_map<std::string, std::vector<std::string>> emotion_keywords_;
    
    // Emotion-to-action mapping table
    std::unordered_map<std::string, std::function<void()>> emotion_actions_;
    
    /**
     * @brief Initialize the emotion-to-action mapping
     */
    void InitializeEmotionActions();
    
    /**
     * @brief Initialize the emotion keyword mapping
     */
    void InitializeEmotionKeywords();
    
    /**
     * @brief Analyze text content
     * @param text The text content
     * @return The detected emotion type
     */
    std::string AnalyzeText(const std::string& text);
    
    /**
     * @brief Execute an emotion action
     * @param emotion The emotion type
     */
    void ExecuteEmotionAction(const std::string& emotion);
};

namespace iot {
    // Declaration of the function that sets the global emotion-response controller pointer
    void SetGlobalEmotionController(EmotionResponseController* controller);
    
    // Declaration of the function that gets the global emotion-response controller pointer
    EmotionResponseController* GetGlobalEmotionController();
} // namespace iot

#endif // EMOTION_RESPONSE_CONTROLLER_H
