# ESP32-S3N16R8-Emoji 

## Hardware requirements

- ESP32 S3 N16R8 development board
- INMP441 MEMS Microphone
- Max98357A I2S audio amplifier
- SSD1306 OLED display (128x64)
- 2 SG90 servos (horizontal and vertical)
- Speaker
- Breadboard and connecting wires

## Features

### 1. Dialogue function
- Support voice conversation
-Support text display
-Support volume adjustment
-Supports key adjustment: volume +/volume- button
- Support voice adjustment: commands such as "Set volume to 50", "Volume to 80" etc.
- Supports multiple volume control command formats:
- "Set volume to xx"
- "Turn the volume to xx"
- "Set volume to xx"
- "Set sound to xx"
- "Volume up"/"Volume up"
- "Volume down"/"Volume down"
- "Mute"/"Turn off sound"
- Support WiFi connection
-Support IoT device control

### 2. Expression mode
- Long press the BOOT button to enter expression mode
- Show cute blinking animation
- Support automatic blink effect (random single blink or two rapid blinks in succession)
- Supports a variety of expression animations: happy, sad, angry, surprised, etc.
- Supports servo control of head movements: nodding, shaking head, swinging, etc.
- The current firmware does not support PAJ7620U2 gesture recognition; do not buy this sensor based on the old instructions and expect plug and play.
- All animations are processed using dedicated tasks to ensure smoothness and naturalness
- Long press the BOOT button again to return to conversation mode

### Expression animation optimization
- Happy expression: optimize the position, size and angle of the triangle to make the expression more natural
- Sad emoticon: achieves precise vertical flipping into a happy emoticon, ensuring perfect symmetry
- Nodding action: improved to nodding up and down multiple times, more in line with natural expression
- Blink animation: Random single blink or two rapid blinks in succession, more vivid and natural
- Use the LVGL graphics library to achieve high-quality expression animation effects
- Ensure expression consistency by accurately calculating triangle coordinates
- Optimize the rotation angle and rotation center point to make the triangle shape more accurate
- Adjust animation parameters to make expression changes more smooth and natural

## Hardware connection

### INMP441 microphone connection

INMP441 is a high-quality I2S digital microphone. The connection method is as follows:

| INMP441 pin | ESP32 S3 N16R8 pin |
|------------|---------------------|
| VDD        | 3.3V                |
| GND        | GND                 |
| SD         | GPIO6              |
| L/R        | GND                 |
| WS         | GPIO4             |
| SCK        | GPIO5             |

### Max98357A audio amplifier connection

Max98357A is an I2S audio amplifier, the connection method is as follows:

| Max98357A pin | ESP32 S3 N16R8 pin |
|-------------|---------------------|
| VIN         | 3.3V          |
| GND         | GND                 |
| DIN         | GPIO7              |
| BCLK        | GPIO15             |
| LRC         | GPIO16              |
| GAIN        | GND           |
| SD          | 3.3V          |

### SSD1306 OLED display connection

| SSD1306 pin | ESP32 S3 N16R8 pin |
|------------|---------------------|
| VCC        | 3.3V                |
| GND        | GND                 |
| SCL        | GPIO42              |
| SDA        | GPIO41              |

### PAJ7620U2 gesture recognition (currently not implemented)

The old version of the document once listed the I2C wiring of PAJ7620U2, but the current firmware has removed the gesture recognition initialization and processing code, and the relevant pin configuration has also been commented. Therefore, GPIO41/42 are currently only used in OLED displays, do not regard this old wiring table as a description of the available functions.

### Servo connection

| Servo | ESP32 S3 N16R8 pins |
|-----------|---------------------|
| Horizontal servo signal line | GPIO11 |
| Vertical servo signal line | GPIO12 |
| VCC        | 5V                  |
| GND        | GND                 |

### Button configuration

| Button | ESP32 S3 N16R8 Pin | Functional Description |
|-----------|---------------------|---------|
| BOOT button | GPIO0 | Short press: switch conversation state<br> Long press: switch expression mode |
| Volume up button | GPIO40 | Short press: Volume +10<br> Long press: Maximum volume |
| Volume down button | GPIO39 | Short press: Volume-10<br> Long press: Mute |

## Instructions for use

### Conversation mode
1. Automatically enter conversation mode after powering on
2. Short press the BOOT button to start the conversation
3. Use the volume buttons to adjust the volume
4. Support WiFi connection and IoT device control

### Expression mode
1. Press and hold the BOOT button to enter expression mode
2. The screen will display two white eyes (white eyes on black background)
3. The eyes will automatically blink every 5 seconds.
4. Press and hold the BOOT button again to return to conversation mode
5. In expression mode, the volume buttons can still adjust the volume
6. Short press the BOOT button to still enter recording mode

## Expression animation system

### Blink animation
- Automatically blink every 5 seconds
- Blink animation simulates natural blink effect
- Blink speed and amplitude are adjustable

### Expression animation
- Happy emoticon: smile effect is displayed under the eyes
- Sad emoticon: Displays sad effect above the eyes
- Angry expression: Angry effect is displayed above the eyes
- Surprise expression: eyes gradually shrink to simulate surprise effect
- Doubtful expression: Move the eyes up and down to simulate a doubtful effect
- Look left expression: Eyes move to the left
- Look right expression: Eyes move to the right
- Sleep expression: eyes become horizontal lines
- Wake-up expression: gradually returns to normal from sleep state

### Servo control
- Head Center: Return the servo to the center position
- Head nod: simulate nodding action
- Head shaking: simulates shaking head movement
- Head rotation: simulates the effect of head rotation
- Head left: simulates looking left
- Head right: simulates looking to the right
- All servo movements adopt smooth transitions to ensure natural flow

### Random expressions and actions
The system will randomly execute the following combination of expressions and actions every 10 seconds to make the expression board more lively and interesting:

1. Blink (wink emoticon + no action) - 60% probability
2. Look left (left expression + servo left) - 15% probability
3. Look to the right (right expression + servo to the right) - 15% probability
4. Happy (happy expression + no action) - 5% probability
5. Turn in circles (default eyes + turning in circles) - 5% probability


### Gesture recognition

Gesture recognition is currently not implemented. The PAJ7620U2 gesture function and corresponding update record in the previous version notes have been removed to avoid inconsistency with the current firmware behavior.

## Function update history (2025-05-12)

### Emotional response system optimization

#### Random animation control
- **Random animations are disabled during the conversation**: During the conversation between the user and the AI ​​(including the user speaking and AI reply phases), the system will automatically disable random expression animations to ensure that the interaction process is not interrupted by random animations
- **Resume random animation after the conversation ends**: When the conversation ends 3 seconds later, the system will automatically restore the random expression animation to keep the expression board lively and interesting in idle state
- **Animation Queue Cleanup**: When disabling random animations, the system will clear all queued animation messages to ensure that there will be no residual random animations executed during the conversation.

#### AI emotional response enhancement
- **Trigger positive emotions when AI replies start**: When AI starts replying, the system will randomly trigger happy or surprised expressions to enhance the vividness of the interaction
- **Random emotions triggered at the end of AI reply**: When the AI ​​reply ends, the system will trigger corresponding emotional expressions based on the reply content, so that the expression board can better express the AI's emotions
- **Restore neutral emotion when conversation ends**: When the conversation is completely over, the system will return to the neutral expression state to prepare for the next interaction

#### Status monitoring optimization
- **Accurate state transition detection**: Optimized the device status monitoring logic to accurately detect the start, continuation and end of the conversation
- **Reduce conversation end judgment delay**: Reduce the conversation end judgment delay from 10 seconds to 3 seconds, allowing the system to return to idle state faster
- **Enhanced logging**: Added detailed status change and animation control logs to facilitate debugging and monitoring system behavior

### Technical implementation
- Use status monitoring tasks to detect device status changes in real time
- Analyze AI reply content and trigger corresponding emotional expressions through emotional response controller
- Use message queue to manage animation requests to ensure the reliability and sequence of animation execution
- Implemented the enable/disable control interface of random animation, which can flexibly control random animation behavior according to context.

##Latest updates

### Emotional response system optimization (2025-05-13)

1. **Stack overflow problem repair**:
- Optimized the emotion command processing logic, replacing regular expressions with simple string searches, significantly reducing stack usage
- Increased the stack size of `ai_response` task from 4096 bytes to 8192 bytes to ensure sufficient stack space
- Fixed a stack overflow issue that may occur when processing special characters

2. **Optimization of steering gear action execution**:
- Improved the servo action execution logic to ensure that the servo action can be executed even if the screen or eye object does not exist
- Optimized the order of execution of the look left and right actions. The servo action is executed first, and then the expression animation is executed.

3. **Emotional word recognition enhancement**:
- Expanded the list of emotional keywords to improve the accuracy of emotion recognition
- Added more Chinese and English emotional expression words to enable the system to recognize more emotional expressions
- Optimized the emotional command processing logic to support more natural language expressions

4. **Technical Implementation**:
- Use friend functions to solve access restriction issues between classes
- Optimized task processing logic to avoid repeated task creation and reduce resource consumption
- Use simple and efficient string processing methods to improve system performance and stability

### Volume control function repair (2025-05-12)

1. **Bug fix**:
- Fixed an issue where volume control via voice commands was ineffective
-Fixed the problem of system crash caused by initialization of IoT function

2. **Volume control enhancement**:
- Supports multiple volume control command formats:
- "Set the volume to xx" (such as "Set the volume to 50")
- "Volume up to xx" (such as "Volume up to 80")
- "Set volume to xx"
- "Set sound to xx"
- "Volume up"/"Volume up"
- "Volume down"/"Volume down"
- "Mute"/"Turn off sound"
-Supports both button control and voice control

3. **Technical Implementation**:
- Identify multiple volume control command formats through regular expressions
- Use AudioCodec and IoT interface (Speaker Thing) to set the volume at the same time
- Safely initialize IoT functions to avoid system crashes
- Add exception handling to improve system stability

4. **How ​​to use**:
- Directly speak volume control commands to Xiaozhi AI, such as "Set volume to 50"
- Or adjust manually using the Volume +/Volume - buttons on your device

## Technical implementation

### Animation management system
- Use dedicated tasks to handle all animation effects
- Use message queue to manage animation requests
- Supports multiple animation types and parameters
- Safe handling of delays and animation execution

### Steering gear control system
- Using PWM to control the steering gear
- Supports smooth transitions and precise control
- Configurable steering gear angle range and speed

### Display system
- Use LVGL graphics library to realize expression display
-Support dynamically adjusting the size and position of emoticons
- Black background with white eyes design to ensure visual effect
