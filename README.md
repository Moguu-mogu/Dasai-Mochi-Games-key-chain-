# Dasai-Mochi ESP32-S3

A small **Dasai-Mochi-inspired desktop game console** built around an
**ESP32-S3 SuperMini**, a 1.3-inch SH1106 OLED, physical buttons, a
touch sensor, and a passive buzzer.

The project combines an animated Mochi face with a separate game menu
and a monochrome arcade shooter called **Sky Patrol**.

> **Status:** Work in progress. The current sketch contains one playable
> game, with the project structured so additional games can be added
> later.

------------------------------------------------------------------------

## Features

### Dasai-Mochi

-   Animated default Mochi face
-   Multiple facial expressions
-   Idle-expression system
-   Idle expression changes every **60 seconds**
-   Idle expressions can repeat in a continuous loop
-   Sleep expression with flat eyes
-   Touch interaction
-   Long-touch return/menu behavior
-   Boot animation and startup sounds

### Game System

-   Separate game menu from the normal Mochi face
-   Current game: **Sky Patrol**
-   Left / Right / Enter controls
-   Approximately 30 FPS game loop
-   Wave-based progression
-   Wave-start delay so the player has time to react
-   Score and lives
-   Power-ups
-   Enemy projectiles
-   Explosions
-   Enemy variety
-   Dreadnought boss every 15 waves

------------------------------------------------------------------------

# Sky Patrol

A small top-down arcade shooter designed specifically for the 128×64
monochrome OLED.

## Controls

  Input                    Action
  ------------------------ -----------------------------
  **Left**                 Move player left
  **Right**                Move player right
  **Enter**                Shoot
  **Hold Touch \~3 sec**   Return to Mochi / exit game

### Power-ups

Power-ups fall into the play area and are activated automatically when
collected.

  Power-up   Effect
  ---------- ----------------------------------------------------------
  Shield     Protects the player
  Tri-Shot   Fires three projectiles
  Bomb       Immediately clears visible enemies and enemy projectiles

The Bomb does **not** require a button combination. Simply collect it to
activate it.

------------------------------------------------------------------------

## Enemies

### Fighter

-   7×6 pixel sprite
-   Faces downward toward the player
-   Basic enemy
-   Straight movement

### Bomber

-   Enlarged pixel-art sprite
-   Enters from the side
-   Moves to an exposed position before leaving
-   Fires randomly
-   Eventually leaves the screen by moving forward/upward
-   Stops normal behavior while leaving

### Scout

-   9×9 pixel sprite
-   Four directional orientations
-   Enters from the left or right
-   Slightly faster than the Bomber
-   Performs randomized moderate circular movements
-   Retains vertical movement
-   Circle radius is intentionally kept moderate
-   Safety logic prevents a circular maneuver from getting too close to
    the player
-   Fires randomly
-   Has lower health than the Bomber
-   Appears in limited numbers and is spaced out within a wave
-   Eventually exits the screen

### Dreadnought

The major boss enemy.

-   Appears every **15 waves**
-   First appears on **Wave 15**
-   Appears again on Waves **30, 45, 60**, etc.
-   28×18 pixel sprite canvas
-   Upward-facing pixel-art design
-   64 HP
-   Worth 15 points when defeated
-   Enters from above the center
-   Uses left, center, and right horizontal positions
-   Changes position after attacks
-   Briefly idles before attacking
-   Uses alternating attack patterns

#### Dreadnought attacks

**Aimed attack** - Large 4×4 circular projectile - Targets the player's
current position when fired - Uses enemy-projectile-style movement speed

**Tri-shot** - Three normal-sized projectiles - Fan-shaped spread -
Alternates with the aimed attack

#### Dreadnought defeat

The boss uses an expanding-circle explosion sequence before
disappearing.

------------------------------------------------------------------------

# Display

The project uses a:

**1.3-inch 128×64 SH1106 I2C OLED**

The monochrome display is intentionally used as a limitation for the
pixel-art style.

## Display specifications

``` text
Resolution: 128 × 64
Controller: SH1106
Interface: I2C
Address: 0x3C / 0x3D
```

------------------------------------------------------------------------

# Hardware

## Main components

-   ESP32-S3 SuperMini
-   1.3-inch SH1106 128×64 OLED
-   3 push buttons
-   Touch sensor
-   Passive buzzer
-   LiPo battery (optional)
-   3.3 V regulated power source

## Pinout

  Component          ESP32-S3 GPIO
  ---------------- ---------------
  OLED SDA                  GPIO 5
  OLED SCL                  GPIO 6
  Touch sensor              GPIO 4
  Passive buzzer            GPIO 7
  Left button               GPIO 1
  Right button              GPIO 2
  Enter button              GPIO 3

Buttons use the ESP32 internal pull-up configuration and are expected to
connect the GPIO to **GND when pressed**.

### Battery

The current sketch does **not** use a battery-voltage sensing GPIO.

``` cpp
#define BATTERY_PIN -1
```

If using a single-cell LiPo, do **not** connect a fully charged 4.2 V
LiPo directly to the ESP32's regulated 3.3 V rail. Use an appropriate
regulator/power-management circuit.

------------------------------------------------------------------------

# Required Arduino Libraries

Install these through the Arduino Library Manager:

-   **Adafruit GFX Library**
-   **Adafruit SH110X**

The sketch also uses standard Arduino/ESP32 functionality such as:

-   `Wire`
-   `math.h`
-   `string.h`

------------------------------------------------------------------------

# Installation

## 1. Install Arduino IDE

Install a recent Arduino IDE with ESP32 board support.

## 2. Install ESP32 support

Add the ESP32 board package through the Arduino Boards Manager.

Select your appropriate:

``` text
ESP32-S3
```

board configuration for your SuperMini.

## 3. Install libraries

Open:

``` text
Arduino IDE
→ Library Manager
```

Install:

``` text
Adafruit GFX Library
Adafruit SH110X
```

## 4. Open the project

Open:

``` text
Dasai-Mochi_Dreadnought_v16.ino
```

The Arduino sketch should be inside a folder with the same name as the
`.ino` file if Arduino IDE requires the standard sketch structure.

## 5. Connect the hardware

Connect the OLED, buttons, touch sensor, and buzzer according to the
pinout above.

## 6. Compile and upload

Select the ESP32-S3 board and upload the sketch.

------------------------------------------------------------------------

# Project Structure

The current project is intentionally kept in one Arduino sketch while
the system is still being developed.

Major sections include:

``` text
Dasai-Mochi
│
├── Hardware / Pin Definitions
├── Button Handling
├── Touch Sensor
├── Sound Effects
├── App State Machine
│
├── Mochi System
│   ├── Eyes
│   ├── Expressions
│   ├── Idle Faces
│   └── Touch Interaction
│
├── Sky Patrol
│   ├── Player
│   ├── Bullets
│   ├── Fighters
│   ├── Bombers
│   ├── Scouts
│   ├── Power-ups
│   ├── Explosions
│   ├── Dreadnought
│   ├── Waves
│   └── Game Over
│
└── Main Loop
```

The game system is designed so additional games can later be added as
separate application states.

------------------------------------------------------------------------

# Pixel Art

The project uses small bitmap-style sprites designed around the 128×64
OLED.

Current approximate sprite sizes:

  Object                  Size
  ------------- --------------
  Player                   7×8
  Fighter                  7×6
  Bomber                 13×10
  Scout                    9×9
  Dreadnought     28×18 canvas

The Dreadnought is intentionally much larger than the normal enemies so
that it reads as a boss on the small display.

------------------------------------------------------------------------

# Wave System

Enemies become more challenging as waves progress.

A short delay is used at the beginning of a new wave:

``` text
New Wave
   ↓
Short reaction period
   ↓
Enemy spawning
   ↓
Wave completed
   ↓
Next Wave
```

The Dreadnought appears on every 15th wave:

``` text
Wave 15
Wave 30
Wave 45
Wave 60
...
```

------------------------------------------------------------------------

# Game Over

When the player loses all lives, the game displays:

``` text
GAME OVER!

Score: XXXX

[ENTER] Retry
```

Press **Enter** to restart.

------------------------------------------------------------------------

# Development Notes

This project is designed around limited hardware and a small monochrome
display.

The main goals are:

-   Simple controls
-   Readable pixel art
-   Lightweight game logic
-   Smooth enough animation on the ESP32-S3
-   Minimal memory usage
-   Easy addition of future games
-   Separation between the normal Mochi experience and games

The project deliberately avoids heavy graphical frameworks such as
pygame. Rendering is performed directly through the OLED graphics
library.

------------------------------------------------------------------------

# Roadmap

Planned or possible future improvements:

-   [ ] Add more games
-   [ ] Add a second/third game to the game menu
-   [ ] Expand the Dreadnought boss system
-   [ ] Add additional boss designs
-   [ ] Add more enemy types
-   [ ] Add more pixel-art animations
-   [ ] Add persistent high scores
-   [ ] Improve sound effects
-   [ ] Add more Mochi idle expressions
-   [ ] Improve power-up balancing
-   [ ] Add battery monitoring hardware/software if desired

------------------------------------------------------------------------

# Credits

**Project:** Dasai-Mochi ESP32-S3 Game Console

Built as a personal embedded-systems and programming project using:

-   ESP32-S3
-   Arduino framework
-   Adafruit GFX
-   Adafruit SH110X
-   SH1106 OLED
-   Custom pixel art
-   C/C++ game logic

------------------------------------------------------------------------

# License

Choose a license before publishing the repository publicly.

For example:

-   **MIT License** --- permissive and simple
-   **GPL-3.0** --- requires derivative projects to remain under GPL
-   **All Rights Reserved** --- if you do not want others reusing the
    code/assets

If the pixel art is original, you should also specify whether other
users are allowed to copy or modify the sprites.

------------------------------------------------------------------------

## Repository suggestion

A clean GitHub repository could eventually look like:

``` text
Dasai-Mochi/
│
├── README.md
├── LICENSE
├── src/
│   └── Dasai-Mochi.ino
│
├── sprites/
│   ├── player/
│   ├── fighter/
│   ├── bomber/
│   ├── scout/
│   └── dreadnought/
│
├── docs/
│   ├── wiring.md
│   └── sprites.md
│
└── images/
    └── project-photo.png
```

This structure will make it much easier to add your other games later
without turning the repository into one large file.
