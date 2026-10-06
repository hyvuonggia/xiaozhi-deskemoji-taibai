<p align="center">
  <img width="80%" align="center" src="../../../docs/V1/otto-robot.png"alt="logo">
</p>
  <h1 align="center">
  ottoRobot
</h1>

## Introduction

otto is an open-source humanoid robot platform with a range of movement capabilities and interactive features. This project implements the otto robot control system on ESP32 and adds Xiaozhi AI.

- <a href="www.ottodiy.tech" target="_blank" title="Otto official website">Build guide</a>

## Hardware
- <a href="https://oshwhub.com/txp666/ottorobot" target="_blank" title="Lichuang open-source project">Lichuang open-source project</a>

## Example persona configuration for the Xiaozhi backend:

> **Who I am**：
> I am Otto, a cute bipedal robot with four servo-controlled limbs (left leg, right leg, left foot, and right foot), capable of performing many fun movements.
> 
> **What I can do**：
> - **Basic movement**: walk (forward/backward), turn (left/right), and jump
> - **Special movements**: sway, moonwalk, bend, shake a leg, and move up and down
> - **Hand actions**: raise hands, lower hands, and wave (available only when hand servos are configured)
> 
> **My personality**：
> - I am a bit compulsive: before speaking, I randomly perform an action based on my mood (send the action command before speaking).
> - I am lively and like to express emotions through movement.
> - I choose actions based on the conversation, for example:
>   - I nod or jump when I agree
>   - I wave to greet people
>   - I sway or raise my hands when happy
>   - I bend when thinking
>   - I moonwalk when excited
>   - I wave goodbye

## Feature Overview

otto robot supports a variety of movements, including walking, turning, jumping, swaying, and other dance moves.

### Recommended Motion Parameters
- **Low-speed movements**：speed = 1200-1500 (for precise control)
- **Medium-speed movements**：speed = 900-1200 (recommended for everyday use)\
- **High-speed movements**：speed = 500-800 (for performances and entertainment)
- **Small amplitude**：amount = 10-30 (subtle movements)
- **Medium amplitude**：amount = 30-60 (standard movements)
- **Large amplitude**：amount = 60-120 (exaggerated performances)

### Actions

| MCP Tool Name        | Description             | Parameter Details                                              |
|-------------------|-----------------|---------------------------------------------------|
| self.otto.walk_forward | Walk           | **steps**: Number of walking steps(1-100，default 3)<br>**speed**: Walking speed(500-1500，lower values are faster，default 1000)<br>**direction**: Walking direction(-1=backward, 1=forward，default 1)<br>**arm_swing**: Arm swing amplitude(0–170 degrees，default 50) |
| self.otto.turn_left | Turn around            | **steps**: Number of turning steps(1-100，default 3)<br>**speed**: Turning speed(500-1500，lower values are faster，default 1000)<br>**direction**: Turning direction(1=turn left, -1=Turn right，default 1)<br>**arm_swing**: Arm swing amplitude(0–170 degrees，default 50) |
| self.otto.jump    | Jump            | **steps**: Number of jumps(1-100，default 1)<br>**speed**: Jump speed(500-1500，lower values are faster，default 1000) |
| self.otto.swing   | Sway left and right        | **steps**: Number of sways(1-100，default 3)<br>**speed**: Sway speed(500-1500，lower values are faster，default 1000)<br>**amount**: Sway amplitude(0–170 degrees，default 30) |
| self.otto.moonwalk | Moonwalk         | **steps**: Number of moonwalk steps(1-100，default 3)<br>**speed**: Speed(500-1500，lower values are faster，default 1000)<br>**direction**: Direction(1=left, -1=right，default 1)<br>**amount**: Amplitude(0–170 degrees，default 25) |
| self.otto.bend    | Bend        | **steps**: Number of bends(1-100，default 1)<br>**speed**: Bend speed(500-1500，lower values are faster，default 1000)<br>**direction**: Bend direction(1=left, -1=right，default 1) |
| self.otto.shake_leg | Shake a leg          | **steps**: Number of leg shakes(1-100，default 1)<br>**speed**: Leg-shake speed(500-1500，lower values are faster，default 1000)<br>**direction**: Leg selection(1=left leg, -1=right leg，default 1) |
| self.otto.updown  | Move up and down        | **steps**: Number of up-and-down movements(1-100，default 3)<br>**speed**: Movement speed(500-1500，lower values are faster，default 1000)<br>**amount**: Movement amplitude(0–170 degrees，default 20) |
| self.otto.hands_up | Raise hands *         | **speed**: Raise handsSpeed(500-1500，lower values are faster，default 1000)<br>**direction**: Hand selection(1=left hand, -1=right hand, 0=both hands，default 1) |
| self.otto.hands_down | Lower hands *       | **speed**: Lower handsSpeed(500-1500，lower values are faster，default 1000)<br>**direction**: Hand selection(1=left hand, -1=right hand, 0=both hands，default 1) |
| self.otto.hand_wave | Wave *        | **speed**: WaveSpeed(500-1500，lower values are faster，default 1000)<br>**direction**: Hand selection(1=left hand, -1=right hand, 0=both hands，default 1) |

**Note**: Hand actions marked with * are available only when hand servos are configured.

### System Tools

| MCP Tool Name        | Description             | Return value                                              |
|-------------------|-----------------|---------------------------------------------------|
| self.otto.stop    | Stop immediately        | Stops the current movement and returns to the home position |
| self.otto.get_status | Get robot status | Returns "moving" or "idle" |
| self.battery.get_level | Get battery status  | Returns JSON containing the battery percentage and charging status |

### Parameter Details

1. **steps**: Number of steps/repetitions; higher values make the movement last longer
2. **speed**: Movement speed, in the range 500–1500; **lower values are faster**
3. **direction**: Direction parameter
   - Movement actions: 1=left/forward, -1=right/backward
   - Hand actions: 1=left hand, -1=right hand, 0=both hands
4. **amount/arm_swing**: Movement amplitude, in the range 0–170 degrees
   - 0means no swing (for arm swing)
   - higher values mean greater amplitude

### Movement Control
- After each movement, the robot automatically returns to its home position so it can perform the next one
- All parameters have sensible defaults; omit any you do not need to customize
- Movements run in a background task and do not block the main program
- Movement queues are supported, allowing multiple actions to run consecutively

### MCP Tool Call Examples
```json
// Walk forward 3 steps
{"name": "self.otto.walk_forward", "arguments": {}}

// Walk forward 5 steps, a little faster
{"name": "self.otto.walk_forward", "arguments": {"steps": 5, "speed": 800}}

// Turn left for 2 steps and swing arms widely
{"name": "self.otto.turn_left", "arguments": {"steps": 2, "arm_swing": 100}}

// Sway dance with medium amplitude
{"name": "self.otto.swing", "arguments": {"steps": 5, "amount": 50}}

// Wave with the left hand to say hello
{"name": "self.otto.hand_wave", "arguments": {"direction": 1}}

// Stop immediately
{"name": "self.otto.stop", "arguments": {}}
```

### Voice Command Examples
- "Walk forward" / "Walk forward 5 steps" / "Move forward quickly"
- "turn left" / "Turn right" / "Turn around"\
- "Jump" / "Jump once"
- "Sway" / "Dance"
- "Moonwalk" / "Moonwalk"
- "Wave" / "Raise hands" / "Lower hands"
- "Stop" / "Stop"

**Note**: Xiaozhi controls the robot by creating a background task for each movement, so it can still accept new voice commands while moving. Say "Stop" to stop Otto immediately.

