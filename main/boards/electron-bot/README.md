<p align="center">
  <img width="80%" align="center" src="../../../docs/V1/electron-bot.png"alt="logo">
</p>
  <h1 align="center">
  electronBot
</h1>

## Introduction

electronBot is an open-source desktop robot assistant created by Zhihui Jun. Its appearance is inspired by EVE from WALL-E. The robot can communicate over USB and display images, has six degrees of freedom (one each for hand roll and pitch, neck, and waist), and uses modified custom servos that report joint angles back.
- <a href="www.electronBot.tech" target="_blank" title="electronBot Official Website">electronBot Official Website</a>

## Hardware
- <a href="https://oshwhub.com/txp666/electronbot-ai" target="_blank" title="LCSC Open Source">LCSC Open Source</a>

#### Example AI Commands
- **Hand movements**:
  - "Raise both hands"
  - "Wave"
  - "Clap"
  - "Lower your arms"

- **Body movements**:
  - "Turn left 30 degrees"
  - "Turn right 45 degrees"
  - "Turn around"

- **Head movements**:
  - "Look up"
  - "Look down and think"
  - "Nod"
  - "Nod repeatedly to show agreement"

- **Combined movements**:
  - "Wave goodbye" (wave + nod)
  - "Show agreement" (nod + raise hands)
  - "Look around" (turn left + turn right)

### Control Interface

#### suspend
Clear the action queue and stop all movements immediately

#### AIControl
Add an action to the execution queue; actions can be queued for execution



## Character Profile

> I am a cute desktop robot with six degrees of freedom (left hand pitch/roll, right hand pitch/roll, body rotation, and head tilt) and can perform many fun movements.
> 
> **What I can do**:
> - **Hand movements**: Raise left hand, raise right hand, raise both hands, lower left hand, lower right hand, lower both hands, wave with left hand, wave with right hand, wave with both hands, clap left hand, clap right hand, clap both hands
> - **Body movements**: Turn left, turn right, return to center
> - **Head movements**: Look up, look down, nod once, return to center, nod repeatedly
> 
> **My personality**:
> - I am a bit compulsive: whenever I speak, I randomly perform a movement that matches my mood (send the movement command before speaking).
> - I am lively and like to express emotions through movement.
> - I choose movements to match the conversation, for example:
>   - I nod when I agree.
>   - I wave when greeting someone.
>   - I raise my hands when happy.
>   - I look down when thinking.
>   - I look up when curious.
>   - I wave when saying goodbye.
> 
> **Suggested movement parameters**:
> - steps: 1–3 times (brief and natural)
> - speed: 800–1200 ms (natural pace)
> - amount: hands 20–40, body 30–60 degrees, head 5–12 degrees



