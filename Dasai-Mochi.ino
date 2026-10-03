// ==================================================
// DASAI MOCHI - MULTI-FEATURE GAME CONSOLE
// Board: ESP32-S3 SuperMini
// SDA: Pin 5 | SCL: Pin 6 | Touch: Pin 4 | Buzzer: Pin 7
// Buttons: Left=Pin 1 | Right=Pin 2 | Enter=Pin 3
// Display: 1.3" SH1106 I2C OLED (128x64)
// Required Library: "Adafruit SH110X" (Install via Arduino Library Manager)
// ==================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <math.h>
#include <string.h>

// --- HARDWARE PIN MAPPING ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SDA_PIN 5
#define SCL_PIN 6
#define TOUCH_PIN 4
#define BUZZER_PIN 7
#define BTN_LEFT 1
#define BTN_RIGHT 2
#define BTN_ENTER 3
#define BATTERY_PIN -1

// Map color definitions
#define SSD1306_WHITE SH110X_WHITE
#define SSD1306_BLACK SH110X_BLACK

// --- DISPLAY DRIVER ---
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
// Arduino auto-generates function prototypes before the rest of the file.
// drawUltraProEye() uses Eye&, so Eye must be forward-declared first.
struct Eye;
void playTone(int frequency, int durationMs);
void playToneAsync(int frequency, int durationMs);
void playBootSound();
void playBootAnimation();
void initMochi();
void initSkyPatrol();
void updateMochi();
void updateSkyPatrol();
void handleMochiTouch();
void handleSkyPatrolInput();
void playerDeath();
void advanceWave();
void drawSkyPatrol();
void drawMenu();
void updateMenu();

// --- BUTTON STATE ---
struct ButtonState {
  bool pressed;       // currently pressed
  bool justPressed;   // edge: just went down this frame
  bool justReleased;  // edge: just released this frame
  unsigned long pressStart;
  bool lastRaw;
  unsigned long lastChange;
};

ButtonState btnLeft = {};
ButtonState btnRight = {};
ButtonState btnEnter = {};

void updateButton(ButtonState &b, int pin) {
  // Buttons are wired from GPIO to GND, so INPUT_PULLUP is used.
  // LOW = physically pressed, HIGH = released.
  bool rawPressed = (digitalRead(pin) == LOW);
  unsigned long now = millis();

  if (rawPressed != b.lastRaw) {
    b.lastChange = now;
    b.lastRaw = rawPressed;
  }

  bool stable = b.pressed;
  if ((now - b.lastChange) >= 25) {
    stable = rawPressed;
  }

  b.justPressed = (stable && !b.pressed);
  b.justReleased = (!stable && b.pressed);
  if (b.justPressed) b.pressStart = now;
  b.pressed = stable;
}

void updateAllButtons() {
  updateButton(btnLeft, BTN_LEFT);
  updateButton(btnRight, BTN_RIGHT);
  updateButton(btnEnter, BTN_ENTER);
}

// --- TOUCH SENSOR FOR BACK-TO-MENU ---
unsigned long touchHoldStart = 0;
bool touchActive = false;
bool touchBackTriggered = false;
const unsigned long TOUCH_BACK_TIME = 3000; // 3 second hold

bool checkTouchBack() {
  bool raw = digitalRead(TOUCH_PIN);
  unsigned long now = millis();
  if (raw && !touchActive) {
    touchHoldStart = now;
    touchActive = true;
    touchBackTriggered = false;
  } else if (raw && touchActive) {
    if (!touchBackTriggered && (now - touchHoldStart) >= TOUCH_BACK_TIME) {
      touchBackTriggered = true;
      return true;
    }
  } else if (!raw) {
    touchActive = false;
    touchBackTriggered = false;
  }
  return false;
}

// --- SOUND EFFECTS ---
void playTone(int freq, int dur) {
  if (BUZZER_PIN < 0) return;
  tone(BUZZER_PIN, freq, dur);
  delay(dur);
  noTone(BUZZER_PIN);
}

void playToneAsync(int freq, int dur) {
  if (BUZZER_PIN < 0) return;
  tone(BUZZER_PIN, freq, dur);
}

void playBootSound() {
  playTone(523, 80);
  playTone(659, 80);
  playTone(784, 80);
  playTone(1047, 120);
}

void playMenuMove() { playToneAsync(1200, 30); }
void playMenuSelect() { playTone(880, 40); playTone(1760, 60); }
void playShoot() { playToneAsync(2000, 20); }
void playHit() { playToneAsync(800, 40); }
void playExplosion() { playTone(200, 60); playTone(100, 80); }
void playPowerUp() { playTone(1047, 50); playTone(1319, 50); playTone(1568, 80); }
void playDeath() { playTone(400, 100); playTone(300, 100); playTone(200, 200); }
void playGameOver() { playTone(392, 200); playTone(330, 200); playTone(262, 400); }

// ============================================================
// APP STATE MACHINE
// ============================================================
enum AppState {
  STATE_GAME_MENU,
  STATE_MOCHI,
  STATE_GAME_SKY_PATROL,
  // Future: STATE_GAME_2, STATE_GAME_3, etc.
};

AppState currentState = STATE_MOCHI;
int menuSelection = 0;

// --- Game Menu Entries ---
#define NUM_GAMES 1
const char* gameNames[NUM_GAMES] = {
  "Sky Patrol"
};

// ============================================================
// BOOT ANIMATION
// ============================================================
void playBootAnimation() {
  display.setTextColor(SH110X_WHITE);
  int cx = 64, cy = 32;
  for (int r = 0; r < 80; r += 4) {
    display.clearDisplay();
    display.fillCircle(cx, cy, r, SH110X_WHITE);
    display.display();
    delay(10);
  }
  for (int r = 0; r < 80; r += 4) {
    display.clearDisplay();
    display.fillCircle(cx, cy, 80, SH110X_WHITE);
    display.fillCircle(cx, cy, r, SH110X_BLACK);
    display.display();
    delay(10);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(30, 24);
  display.print("DASAI MOCHI");
  display.setCursor(40, 40);
  display.setTextSize(1);
  display.print("v2.0");
  display.display();
  delay(1200);
}

// ============================================================
// SEPARATE GAME MENU
// ============================================================
void drawGameMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // Title bar with line
  display.setCursor(20, 2);
  display.print("GAMES");
  display.drawLine(0, 12, 127, 12, SH110X_WHITE);

  // Game list
  for (int i = 0; i < NUM_GAMES; i++) {
    int y = 16 + i * 12;
    if (i == menuSelection) {
      // Selection indicator
      display.setCursor(2, y);
      display.print(">");
      // Highlight bar
      display.fillRect(10, y - 1, 118, 11, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
      display.setCursor(14, y);
      display.print(gameNames[i]);
      display.setTextColor(SH110X_WHITE);
    } else {
      display.setCursor(14, y);
      display.print(gameNames[i]);
    }
  }

  // Bottom bar - navigation hints
  display.drawLine(0, 53, 127, 53, SH110X_WHITE);

  // Game menu controls
  display.setCursor(2, 56);
  display.print("LEFT/RIGHT");
  display.setCursor(88, 56);
  display.print("SELECT");

  display.display();
}

bool menuConfirmActive = false;

void updateGameMenu() {
  if (btnLeft.justPressed) {
    menuSelection--;
    if (menuSelection < 0) menuSelection = NUM_GAMES - 1;
    playMenuMove();
  }

  if (btnRight.justPressed) {
    menuSelection++;
    if (menuSelection >= NUM_GAMES) menuSelection = 0;
    playMenuMove();
  }

  if (btnEnter.justPressed) {
    playMenuSelect();
    switch (menuSelection) {
      case 0:
        currentState = STATE_GAME_SKY_PATROL;
        initSkyPatrol();
        break;
    }
  }

  drawGameMenu();
}

// ============================================================
// MOCHI PET (Existing animated eyes)
// ============================================================

// --- EYE LAYOUT CONSTANTS ---
#define LEFT_EYE_X      18
#define RIGHT_EYE_X     74
#define EYE_Y           14
#define EYE_DEFAULT_W   36
#define EYE_DEFAULT_H   36

struct Eye {
  float x, y, w, h;
  float targetX, targetY, targetW, targetH;
  float pupilX, pupilY;
  float targetPupilX, targetPupilY;
  float velX, velY, velW, velH;
  float pVelX, pVelY;
  float k = 0.12;
  float d = 0.60;
  float pk = 0.08;
  float pd = 0.50;
  bool blinking;
  unsigned long lastBlink;
  unsigned long nextBlinkTime;

  void init(float _x, float _y, float _w, float _h) {
    x = targetX = _x; y = targetY = _y;
    w = targetW = _w; h = targetH = _h;
    pupilX = targetPupilX = 0;
    pupilY = targetPupilY = 0;
    velX = velY = velW = velH = 0;
    pVelX = pVelY = 0;
    nextBlinkTime = millis() + random(1000, 4000);
    blinking = false;
  }

  void update() {
    float ax = (targetX - x) * k;
    float ay = (targetY - y) * k;
    float aw = (targetW - w) * k;
    float ah = (targetH - h) * k;
    velX = (velX + ax) * d;
    velY = (velY + ay) * d;
    velW = (velW + aw) * d;
    velH = (velH + ah) * d;
    x += velX; y += velY; w += velW; h += velH;

    float pax = (targetPupilX - pupilX) * pk;
    float pay = (targetPupilY - pupilY) * pk;
    pVelX = (pVelX + pax) * pd;
    pVelY = (pVelY + pay) * pd;
    pupilX += pVelX;
    pupilY += pVelY;
  }
};

Eye leftEye, rightEye;
unsigned long lastSaccade = 0;
unsigned long saccadeInterval = 3000;
float breathVal = 0;

#define MOOD_NORMAL 0
#define MOOD_HAPPY 1
#define MOOD_SURPRISED 2
#define MOOD_SLEEPY 3
#define MOOD_ANGRY 4
#define MOOD_SAD 5
#define MOOD_EXCITED 6
#define MOOD_LOVE 7
#define MOOD_SUSPICIOUS 8
#define MOOD_LOW_BATTERY 9
int currentMood = MOOD_NORMAL;

// --- AUTOMATIC MOCHI IDLE EXPRESSIONS ---
unsigned long mochiIdleTimer = 0;
int mochiIdleMood = MOOD_NORMAL;
int mochiPreviousMood = MOOD_NORMAL;
uint8_t mochiIdleIndex = 0;
const unsigned long MOCHI_IDLE_INTERVAL = 60000UL;

const uint8_t MOCHI_IDLE_MOODS[] = {
  MOOD_SLEEPY, MOOD_HAPPY, MOOD_SURPRISED, MOOD_SAD,
  MOOD_SUSPICIOUS, MOOD_EXCITED, MOOD_LOVE, MOOD_ANGRY
};
const uint8_t MOCHI_IDLE_MOOD_COUNT = sizeof(MOCHI_IDLE_MOODS) / sizeof(MOCHI_IDLE_MOODS[0]);

// Emotion bitmaps
const unsigned char bmp_heart[] PROGMEM = { 0x00,0x00,0x0c,0x60,0x1e,0xf0,0x3f,0xf8,0x7f,0xfc,0x7f,0xfc,0x7f,0xfc,0x3f,0xf8,0x1f,0xf0,0x0f,0xe0,0x07,0xc0,0x03,0x80,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
const unsigned char bmp_zzz[] PROGMEM = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3c,0x00,0x0c,0x00,0x18,0x00,0x30,0x00,0x7e,0x00,0x00,0x3c,0x00,0x0c,0x00,0x18,0x00,0x30,0x00,0x7c,0x00,0x00,0x00,0x00,0x00 };
const unsigned char bmp_anger[] PROGMEM = { 0x00,0x00,0x11,0x10,0x2a,0x90,0x44,0x40,0x80,0x20,0x80,0x20,0x44,0x40,0x2a,0x90,0x11,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

// Mochi touch interaction
int mochiTapCounter = 0;
unsigned long mochiLastTapTime = 0;
bool mochiLastPinState = false;
unsigned long mochiPressStart = 0;
bool mochiLongPressHandled = false;

void initMochi() {
  leftEye.init(LEFT_EYE_X, EYE_Y, EYE_DEFAULT_W, EYE_DEFAULT_H);
  rightEye.init(RIGHT_EYE_X, EYE_Y, EYE_DEFAULT_W, EYE_DEFAULT_H);
  currentMood = MOOD_NORMAL;
  mochiIdleMood = MOOD_NORMAL;
  mochiPreviousMood = MOOD_NORMAL;
  mochiIdleTimer = millis();
  lastSaccade = 0;
  mochiTapCounter = 0;
}

void drawEyelidMask(float x, float y, float w, float h, int mood, bool isLeft) {
  int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
  if (mood == MOOD_ANGRY) {
    if (isLeft) for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, SH110X_BLACK);
    else for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, SH110X_BLACK);
  } else if (mood == MOOD_SAD) {
    if (isLeft) for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, SH110X_BLACK);
    else for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, SH110X_BLACK);
  } else if (mood == MOOD_HAPPY || mood == MOOD_LOVE || mood == MOOD_EXCITED) {
    display.fillRect(ix, iy + ih - 12, iw, 14, SH110X_BLACK);
    display.fillCircle(ix + iw / 2, iy + ih + 6, iw / 1.3, SH110X_BLACK);
  } else if (mood == MOOD_SLEEPY) {
    // Sleep: completely flat/closed eyes. Draw a single clean horizontal
    // lid rather than leaving a visible pupil or half-open eye.
    int lineY = iy + ih / 2;
    display.drawLine(ix + 2, lineY, ix + iw - 3, lineY, SH110X_BLACK);
    display.drawLine(ix + 4, lineY + 1, ix + iw - 5, lineY + 1, SH110X_BLACK);
  } else if (mood == MOOD_LOW_BATTERY) {
    display.fillRect(ix, iy, iw, ih / 2 + 4, SH110X_BLACK);
  } else if (mood == MOOD_SUSPICIOUS) {
    if (isLeft) display.fillRect(ix, iy, iw, ih / 2 - 2, SH110X_BLACK);
    else display.fillRect(ix, iy + ih - 8, iw, 8, SH110X_BLACK);
  }
}

void drawUltraProEye(Eye& e, bool isLeft, int mood) {
  int ix = (int)e.x, iy = (int)e.y, iw = (int)e.w, ih = (int)e.h;

  // Sleep face: draw only a clean, flat closed-eye line.
  // This prevents the normal white eye/pupil from showing through.
  if (mood == MOOD_SLEEPY) {
    int lineY = iy + ih / 2;
    display.drawLine(ix + 3, lineY, ix + iw - 4, lineY, SH110X_WHITE);
    display.drawLine(ix + 5, lineY + 1, ix + iw - 6, lineY + 1, SH110X_WHITE);
    return;
  }

  int r = (iw < 20) ? 3 : 8;
  display.fillRoundRect(ix, iy, iw, ih, r, SH110X_WHITE);
  int cx = ix + iw / 2, cy = iy + ih / 2;
  int pw = iw / 2.2, ph = ih / 2.2;
  if (pw > iw) pw = max(1, iw);
  if (ph > ih) ph = max(1, ih);
  int px = cx + (int)e.pupilX - (pw / 2);
  int py = cy + (int)e.pupilY - (ph / 2);
  if (px < ix) px = ix;
  if (px + pw > ix + iw) px = ix + iw - pw;
  if (py < iy) py = iy;
  if (py + ph > iy + ih) py = iy + ih - ph;
  display.fillRoundRect(px, py, pw, ph, r / 2, SH110X_BLACK);
  if (iw > 15 && ih > 15) display.fillCircle(px + pw - 4, py + 4, 2, SH110X_WHITE);
  drawEyelidMask(e.x, e.y, e.w, e.h, mood, isLeft);
}

void updateMochiPhysics() {
  unsigned long now = millis();
  breathVal = sin(now / 800.0) * 1.5;

  if (now > leftEye.nextBlinkTime) {
    leftEye.blinking = true; leftEye.lastBlink = now;
    rightEye.blinking = true;
    leftEye.nextBlinkTime = now + random(2000, 6000);
  }
  if (leftEye.blinking) {
    leftEye.targetH = 2; rightEye.targetH = 2;
    if (now - leftEye.lastBlink > 120) {
      leftEye.blinking = false; rightEye.blinking = false;
    }
  }

  if (!leftEye.blinking && now - lastSaccade > saccadeInterval) {
    lastSaccade = now;
    saccadeInterval = random(500, 3000);
    int dir = random(0, 10);
    float lx = 0, ly = 0;
    if (dir >= 4 && dir <= 5) { lx = (dir == 4) ? -6 : 6; ly = -4; }
    else if (dir >= 6 && dir <= 7) { lx = (dir == 6) ? -6 : 6; ly = 4; }
    else if (dir == 8) { lx = 8; }
    else if (dir == 9) { lx = -8; }
    leftEye.targetPupilX = lx; leftEye.targetPupilY = ly;
    rightEye.targetPupilX = lx; rightEye.targetPupilY = ly;
    leftEye.targetX = LEFT_EYE_X + lx * 0.3;
    leftEye.targetY = EYE_Y + ly * 0.3;
    rightEye.targetX = RIGHT_EYE_X + lx * 0.3;
    rightEye.targetY = EYE_Y + ly * 0.3;
  }

  if (!leftEye.blinking) {
    float bW = EYE_DEFAULT_W, bH = EYE_DEFAULT_H + breathVal;
    switch (currentMood) {
      case MOOD_NORMAL: leftEye.targetW = bW; leftEye.targetH = bH; rightEye.targetW = bW; rightEye.targetH = bH; break;
      case MOOD_HAPPY: case MOOD_LOVE: leftEye.targetW = 40; leftEye.targetH = 32; rightEye.targetW = 40; rightEye.targetH = 32; break;
      case MOOD_SURPRISED: leftEye.targetW = 30; leftEye.targetH = 45; rightEye.targetW = 30; rightEye.targetH = 45; break;
      case MOOD_SLEEPY: leftEye.targetW = 40; leftEye.targetH = 18; rightEye.targetW = 40; rightEye.targetH = 18; break;
      case MOOD_ANGRY: leftEye.targetW = 34; leftEye.targetH = 32; rightEye.targetW = 34; rightEye.targetH = 32; break;
      case MOOD_SAD: leftEye.targetW = 34; leftEye.targetH = 40; rightEye.targetW = 34; rightEye.targetH = 40; break;
      case MOOD_SUSPICIOUS: leftEye.targetW = 36; leftEye.targetH = 20; rightEye.targetW = 36; rightEye.targetH = 42; break;
      case MOOD_EXCITED: leftEye.targetW = 42; leftEye.targetH = 40 + breathVal * 2; rightEye.targetW = 42; rightEye.targetH = 40 + breathVal * 2; break;
      case MOOD_LOW_BATTERY: leftEye.targetW = 38; leftEye.targetH = 24; rightEye.targetW = 38; rightEye.targetH = 24; break;
    }
  }
  leftEye.update();
  rightEye.update();
}

void updateMochiIdle() {
  unsigned long now = millis();

  // Change the face exactly once every 60 seconds.
  // The expressions run in a repeating loop, so repeats are allowed.
  // Example: SLEEP -> HAPPY -> SURPRISED -> SAD -> ... -> SLEEP -> ...
  if ((now - mochiIdleTimer) >= MOCHI_IDLE_INTERVAL) {
    mochiIdleIndex = (mochiIdleIndex + 1) % MOCHI_IDLE_MOOD_COUNT;

    int nextMood = MOCHI_IDLE_MOODS[mochiIdleIndex];
    mochiPreviousMood = nextMood;
    mochiIdleMood = nextMood;
    currentMood = nextMood;
    mochiIdleTimer = now;

    // Start the new expression with a natural eye movement.
    lastSaccade = now;
    saccadeInterval = random(700, 1800);
  }
}

void handleMochiTouch() {
  // Normal Mochi mode intentionally does not use the game buttons.
  // The face remains the default Mochi screen.
}

void updateMochi() {
  handleMochiTouch();
  updateMochiIdle();
  updateMochiPhysics();
  display.clearDisplay();
  int mood = currentMood;
  if (mood == MOOD_LOVE) display.drawBitmap(56, 0, bmp_heart, 16, 16, SH110X_WHITE);
  else if (mood == MOOD_SLEEPY) display.drawBitmap(110, 0, bmp_zzz, 16, 16, SH110X_WHITE);
  else if (mood == MOOD_ANGRY) display.drawBitmap(56, 0, bmp_anger, 16, 16, SH110X_WHITE);
  else if (mood == MOOD_EXCITED) {
    unsigned long t = millis();
    display.fillCircle(52 + (t / 100) % 6, 4, 1, SH110X_WHITE);
    display.fillCircle(64, 2 + (t / 150) % 5, 1, SH110X_WHITE);
    display.fillCircle(74 - (t / 120) % 6, 6, 1, SH110X_WHITE);
  }
  drawUltraProEye(leftEye, true, mood);
  drawUltraProEye(rightEye, false, mood);

  display.display();
}

// ============================================================
// SKY PATROL - 1942-Style Vertical Shooter
// ============================================================

// --- Game Constants ---
#define SP_PLAYER_W 7
#define SP_PLAYER_H 8
#define SP_PLAYER_Y (SCREEN_HEIGHT - SP_PLAYER_H - 2)
#define SP_MAX_BULLETS 12
#define SP_MAX_ENEMIES 16
#define SP_SCOUT_MAX_PER_WAVE 4
#define SP_MAX_ENEMY_BULLETS 8
#define SP_MAX_POWERUPS 3
#define SP_MAX_EXPLOSIONS 6
#define SP_MAX_STARS 20

// Player sprite (7x8) - forward-facing fighter plane
const unsigned char spr_player[] PROGMEM = {
  0b00010000,  //    *
  0b00111000,  //   ***
  0b00111000,  //   ***
  0b11111110,  //  *******
  0b11111110,  //  *******
  0b00111000,  //   ***
  0b01010100,  //  * * *
  0b01010100   //  * * *
};

// Sprite artwork matches the user's 128x64 pixel-grid reference exactly.
// Fighter: 7x6, always facing DOWN toward the player.
const uint8_t spr_fighter[6] PROGMEM = {
  0b01111100,  // .#####.
  0b00111000,  // ..###..
  0b11111110,  // #######
  0b11111110,  // #######
  0b00111000,  // ..###..
  0b00010000   // ...#...
};

// Bomber: 11x9, exact silhouette from the reference.
const uint16_t spr_bomber[9] PROGMEM = {
  0b00000100000, // .....#.....
  0b00001110000, // ....###....
  0b01111011110, // .####.####.
  0b11111111111, // ###########
  0b11111111111, // ###########
  0b01101110110, // .##.###.##.
  0b00001110000, // ....###....
  0b00011111000, // ...#####...
  0b00011111000  // ...#####...
};

// Scout: 9x9. This base orientation is the exact reference sprite.
// drawScout() rotates it in 90-degree steps, producing the other three
// reference orientations without needing four separate bitmaps.
const uint16_t spr_scout[9] PROGMEM = {
  0b000010000, // ....#....
  0b000111000, // ...###...
  0b000101000, // ...#.#...
  0b111111111, // #########
  0b111111111, // #########
  0b001111100, // ..#####..
  0b000010000, // ....#....
  0b000111000, // ...###...
  0b001111100  // ..#####..
};

// --- Game Structures ---
struct Bullet {
  float x, y;
  float dx, dy;
  bool active;
};

struct Enemy {
  float x, y;
  float dx, dy;
  int hp;
  int type; // 0 = fighter, 1 = bomber, 2 = scout
  bool active;
  unsigned long spawnTime;
  unsigned long lastShot;
  unsigned long shotInterval;
  float idlePhase;

  // Bomber target position
  float targetX, targetY;

  // Scout movement state
  int movePhase;       // 0 enter, 1 circle, 2 straight, 3 circle, 4 exit
  float circleCX, circleCY;
  float circleRadius;
  float circleAngle;
  float circleStartAngle;
  float straightRemaining;
  int spriteDir;       // 0 up, 1 right, 2 down, 3 left
  int circlesDone;
};

struct PowerUp {
  float x, y;
  int type; // 0 = shield, 1 = trishot, 2 = bomb
  bool active;
};

struct Explosion {
  float x, y;
  int frame;
  bool active;
};

struct Star {
  float x, y;
  float speed;
};

// --- Game State ---
struct SkyPatrolGame {
  // Player
  float playerX;
  int lives;
  int score;
  bool shieldActive;
  unsigned long shieldEnd;
  bool triShotActive;
  unsigned long triShotEnd;
  int bombCount;

  // Wave system
  int wave;
  int difficulty;
  int enemiesSpawned;
  int enemiesPerWave;
  int enemiesKilledThisWave;
  unsigned long lastEnemySpawn;
  unsigned long waveStartTime;
  unsigned long spawnUnlockTime; // delay before enemies can appear in a new wave
  int spawnInterval; // ms between enemy spawns

  // Bomber/scout tracking
  int activeBombers;
  int scoutsSpawnedThisWave;

  // Game state
  bool gameOver;
  bool paused;
  unsigned long gameStartTime;
  unsigned long lastFrame;

  // Invincibility after death
  bool invincible;
  unsigned long invincibleEnd;

  // Shoot cooldown
  unsigned long lastShot;

  Bullet bullets[SP_MAX_BULLETS];
  Bullet enemyBullets[SP_MAX_ENEMY_BULLETS];
  Enemy enemies[SP_MAX_ENEMIES];
  PowerUp powerups[SP_MAX_POWERUPS];
  Explosion explosions[SP_MAX_EXPLOSIONS];
  Star stars[SP_MAX_STARS];
};

SkyPatrolGame sp;

void initSkyPatrol() {
  memset(&sp, 0, sizeof(sp));
  sp.playerX = SCREEN_WIDTH / 2 - SP_PLAYER_W / 2;
  sp.lives = 3;
  sp.score = 0;
  sp.wave = 1;
  sp.difficulty = 1;
  sp.enemiesPerWave = 5;
  sp.spawnInterval = 1200;
  sp.gameStartTime = millis();
  sp.waveStartTime = millis();
  sp.spawnUnlockTime = millis() + 1500;
  sp.lastEnemySpawn = millis() - sp.spawnInterval;
  sp.lastFrame = millis();
  sp.lastShot = 0;
  sp.bombCount = 0;
  sp.activeBombers = 0;
  sp.scoutsSpawnedThisWave = 0;

  // Init stars
  for (int i = 0; i < SP_MAX_STARS; i++) {
    sp.stars[i].x = random(0, SCREEN_WIDTH);
    sp.stars[i].y = random(0, SCREEN_HEIGHT);
    sp.stars[i].speed = 0.3 + (random(0, 10) / 10.0);
  }
}

// --- Draw Player ---
void drawPlayer() {
  if (sp.gameOver) return;
  int px = (int)sp.playerX;
  int py = SP_PLAYER_Y;

  // Blink during invincibility
  if (sp.invincible && ((millis() / 100) % 2 == 0)) return;

  // Draw player ship procedurally
  // Fuselage
  display.fillRect(px + 2, py, 3, SP_PLAYER_H, SH110X_WHITE);
  // Wings
  display.fillRect(px, py + 3, SP_PLAYER_W, 2, SH110X_WHITE);
  // Nose
  display.drawPixel(px + 3, py - 1, SH110X_WHITE);
  // Tail fins
  display.drawPixel(px + 1, py + SP_PLAYER_H - 1, SH110X_WHITE);
  display.drawPixel(px + 5, py + SP_PLAYER_H - 1, SH110X_WHITE);

  // Shield bubble
  if (sp.shieldActive) {
    display.drawCircle(px + 3, py + 3, 7, SH110X_WHITE);
    // Flicker during the final 3 seconds.
    unsigned long now = millis();
    if (now < sp.shieldEnd && (sp.shieldEnd - now) < 3000) {
      if ((now / 200) % 2 == 0)
        display.drawCircle(px + 3, py + 3, 6, SH110X_WHITE);
    }
  }
}

// --- Draw Fighter Enemy ---
void drawFighter(int x, int y) {
  // Exact 7x6 reference sprite.
  for (int row = 0; row < 6; row++) {
    uint8_t bits = pgm_read_byte(&spr_fighter[row]);
    for (int col = 0; col < 7; col++) {
      if (bits & (0x40 >> col)) {
        display.drawPixel(x + col, y + row, SH110X_WHITE);
      }
    }
  }
}

// --- Draw Bomber Enemy ---
void drawBomber(int x, int y) {
  // Exact 11x9 reference sprite.
  for (int row = 0; row < 9; row++) {
    uint16_t bits = pgm_read_word(&spr_bomber[row]);
    for (int col = 0; col < 11; col++) {
      if (bits & (1U << (10 - col))) {
        display.drawPixel(x + col, y + row, SH110X_WHITE);
      }
    }
  }
}

// --- Draw Scout Enemy ---
void drawScout(int x, int y, int dir) {
  // Rotate the 9x9 base sprite in 90-degree steps.
  for (int sy = 0; sy < 9; sy++) {
    uint16_t rowBits = pgm_read_word(&spr_scout[sy]);
    for (int sx = 0; sx < 9; sx++) {
      if (!(rowBits & (1U << (8 - sx)))) continue;

      int dx = sx;
      int dy = sy;
      if (dir == 1) {       // right
        dx = 8 - sy; dy = sx;
      } else if (dir == 2) { // down
        dx = 8 - sx; dy = 8 - sy;
      } else if (dir == 3) { // left
        dx = sy; dy = 8 - sx;
      }
      display.drawPixel(x + dx, y + dy, SH110X_WHITE);
    }
  }
}

// --- Draw Stars Background ---
void drawStars() {
  for (int i = 0; i < SP_MAX_STARS; i++) {
    display.drawPixel((int)sp.stars[i].x, (int)sp.stars[i].y, SH110X_WHITE);
  }
}

void updateStars() {
  for (int i = 0; i < SP_MAX_STARS; i++) {
    sp.stars[i].y += sp.stars[i].speed;
    if (sp.stars[i].y >= SCREEN_HEIGHT) {
      sp.stars[i].y = 0;
      sp.stars[i].x = random(0, SCREEN_WIDTH);
    }
  }
}

// --- Spawn Helpers ---
void spawnBullet(float x, float y, float dx, float dy) {
  for (int i = 0; i < SP_MAX_BULLETS; i++) {
    if (!sp.bullets[i].active) {
      sp.bullets[i] = {x, y, dx, dy, true};
      return;
    }
  }
}

void spawnEnemyBullet(float x, float y) {
  for (int i = 0; i < SP_MAX_ENEMY_BULLETS; i++) {
    if (!sp.enemyBullets[i].active) {
      sp.enemyBullets[i] = {x, y, 0, 1.5, true};
      return;
    }
  }
}

void spawnExplosion(float x, float y) {
  for (int i = 0; i < SP_MAX_EXPLOSIONS; i++) {
    if (!sp.explosions[i].active) {
      sp.explosions[i] = {x, y, 0, true};
      return;
    }
  }
}

void spawnPowerUp(float x, float y) {
  // Random chance to drop a power-up (25%)
  if (random(0, 100) >= 25) return;
  for (int i = 0; i < SP_MAX_POWERUPS; i++) {
    if (!sp.powerups[i].active) {
      sp.powerups[i].x = x;
      sp.powerups[i].y = y;
      sp.powerups[i].type = random(0, 3); // 0=shield, 1=trishot, 2=bomb
      sp.powerups[i].active = true;
      return;
    }
  }
}

void spawnEnemy() {
  for (int i = 0; i < SP_MAX_ENEMIES; i++) {
    if (!sp.enemies[i].active) {
      bool spawnBomber = false;
      bool spawnScout = false;

      // Scouts appear from wave 3 onward, 1-4 per wave.
      // Bombers remain rarer and tougher.
      int scoutQuota = 0;
      if (sp.wave >= 3) scoutQuota = min(4, 1 + ((sp.wave - 3) / 2));
      if (sp.wave >= 3 && sp.scoutsSpawnedThisWave < scoutQuota) {
        // Guarantee the quota while still allowing the exact spawn order to vary.
        int remainingEnemies = sp.enemiesPerWave - sp.enemiesSpawned;
        int scoutsNeeded = scoutQuota - sp.scoutsSpawnedThisWave;
        if (remainingEnemies <= scoutsNeeded || random(0, 100) < min(50, 18 + sp.wave * 2)) {
          spawnScout = true;
        }
      }

      if (!spawnScout && sp.difficulty > 6 && sp.activeBombers < 2) {
        if (random(0, 100) < 20) spawnBomber = true;
      }

      unsigned long now = millis();
      sp.enemies[i].spawnTime = now;
      sp.enemies[i].lastShot = now;
      sp.enemies[i].shotInterval = random(2600, 4301);
      sp.enemies[i].active = true;
      sp.enemies[i].idlePhase = random(0, 628) / 100.0;
      sp.enemies[i].targetX = 0;
      sp.enemies[i].targetY = 0;
      sp.enemies[i].movePhase = 0;
      sp.enemies[i].circleCX = 0;
      sp.enemies[i].circleCY = 0;
      sp.enemies[i].circleRadius = 0;
      sp.enemies[i].circleAngle = 0;
      sp.enemies[i].circleStartAngle = 0;
      sp.enemies[i].straightRemaining = 0;
      sp.enemies[i].spriteDir = 0;
      sp.enemies[i].circlesDone = 0;

      if (spawnScout) {
        // Scout enters horizontally from a side, like the bomber.
        bool fromLeft = random(0, 2) == 0;
        sp.enemies[i].type = 2;
        sp.enemies[i].hp = 3;
        sp.enemies[i].x = fromLeft ? -9 : SCREEN_WIDTH;
        sp.enemies[i].y = random(8, 43);
        sp.enemies[i].dx = fromLeft ? 0.3 : -0.3;
        sp.enemies[i].dy = 0;
        sp.enemies[i].spriteDir = fromLeft ? 1 : 3;
        sp.enemies[i].movePhase = 0;
        sp.enemies[i].straightRemaining = random(12, 29);
        sp.enemies[i].circleRadius = random(7, 11);
        sp.enemies[i].circleCX = fromLeft ? random(18, 72) : random(56, 110);
        // Allow some close passes, but never put the circle center directly on the player.
        sp.enemies[i].circleCY = random(18, 49);
        sp.enemies[i].circleStartAngle = random(0, 628) / 100.0;
        sp.enemies[i].circleAngle = sp.enemies[i].circleStartAngle;
        sp.enemies[i].circlesDone = 0;
        sp.scoutsSpawnedThisWave++;
      } else if (spawnBomber) {
        sp.enemies[i].type = 1;
        sp.enemies[i].hp = 5;
        bool fromLeft = random(0, 2) == 0;
        sp.enemies[i].x = fromLeft ? -11 : SCREEN_WIDTH;
        sp.enemies[i].y = random(6, 32);
        // More exposed random idle target instead of hugging its entrance side.
        if (random(0, 100) < 65) {
          sp.enemies[i].targetX = random(16, SCREEN_WIDTH - 27);
        } else {
          sp.enemies[i].targetX = fromLeft ? random(4, 22) : random(SCREEN_WIDTH - 28, SCREEN_WIDTH - 12);
        }
        sp.enemies[i].targetY = random(8, 34); // safely above player
        sp.enemies[i].dx = fromLeft ? 0.3 : -0.3;
        sp.enemies[i].dy = 0;
        sp.activeBombers++;
      } else {
        sp.enemies[i].type = 0;
        sp.enemies[i].hp = 1;
        sp.enemies[i].x = random(2, SCREEN_WIDTH - 7);
        sp.enemies[i].y = -6;
        sp.enemies[i].dx = 0;
        sp.enemies[i].dy = 0.5 + (sp.difficulty * 0.05);
        if (sp.enemies[i].dy > 1.2) sp.enemies[i].dy = 1.2;
      }

      sp.enemiesSpawned++;
      return;
    }
  }
}

// --- Player Input ---
void handleSkyPatrolInput() {
  if (sp.gameOver) {
    if (btnEnter.justPressed) {
      initSkyPatrol(); // restart
    }
    return;
  }

  // Movement: hold left/right for continuous movement
  float moveSpeed = 2.0;
  if (btnLeft.pressed) {
    sp.playerX -= moveSpeed;
    if (sp.playerX < 0) sp.playerX = 0;
  }
  if (btnRight.pressed) {
    sp.playerX += moveSpeed;
    if (sp.playerX > SCREEN_WIDTH - SP_PLAYER_W) sp.playerX = SCREEN_WIDTH - SP_PLAYER_W;
  }

  // Bomb: hold both Left+Right and press Enter.
  // This takes priority over shooting so one Enter press cannot do both.
  bool useBomb = btnLeft.pressed && btnRight.pressed &&
                 btnEnter.justPressed && sp.bombCount > 0;

  if (useBomb) {
    sp.bombCount--;
    // Destroy every enemy currently visible on the playfield.
    // Enemies still off-screen remain alive and can enter later.
    for (int i = 0; i < SP_MAX_ENEMIES; i++) {
      if (!sp.enemies[i].active) continue;
      int ew = (sp.enemies[i].type == 0) ? 7 : (sp.enemies[i].type == 1 ? 11 : 9);
      int eh = (sp.enemies[i].type == 0) ? 6 : 9;
      bool visible = (sp.enemies[i].x + ew > 0 && sp.enemies[i].x < SCREEN_WIDTH &&
                      sp.enemies[i].y + eh > 0 && sp.enemies[i].y < SCREEN_HEIGHT);
      if (visible) {
        spawnExplosion(sp.enemies[i].x, sp.enemies[i].y);
        sp.score += (sp.enemies[i].type == 1) ? 3 : (sp.enemies[i].type == 2 ? 2 : 1);
        sp.enemiesKilledThisWave++;
        if (sp.enemies[i].type == 1) sp.activeBombers--;
        sp.enemies[i].active = false;
      }
    }
    // Clear enemy bullets too
    for (int i = 0; i < SP_MAX_ENEMY_BULLETS; i++) {
      sp.enemyBullets[i].active = false;
    }
    playExplosion();
    // Flash screen effect
  } else if (btnEnter.justPressed) {
    // Fire: single press each shot
    unsigned long now = millis();
    if (now - sp.lastShot > 150) {
      sp.lastShot = now;

      float bx = sp.playerX + SP_PLAYER_W / 2;
      float by = SP_PLAYER_Y - 2;

      if (sp.triShotActive && millis() < sp.triShotEnd) {
        spawnBullet(bx, by, 0, -3);
        spawnBullet(bx, by, -1, -2.8);
        spawnBullet(bx, by, 1, -2.8);
      } else {
        sp.triShotActive = false;
        spawnBullet(bx, by, 0, -3);
      }
      playShoot();
    }
  }
}

// --- Update Game Logic ---
void updateSkyPatrol() {
  unsigned long now = millis();

  // Update bullets
  for (int i = 0; i < SP_MAX_BULLETS; i++) {
    if (sp.bullets[i].active) {
      sp.bullets[i].x += sp.bullets[i].dx;
      sp.bullets[i].y += sp.bullets[i].dy;
      if (sp.bullets[i].y < -2 || sp.bullets[i].x < -2 || sp.bullets[i].x > SCREEN_WIDTH + 2)
        sp.bullets[i].active = false;
    }
  }

  // Update enemy bullets
  for (int i = 0; i < SP_MAX_ENEMY_BULLETS; i++) {
    if (sp.enemyBullets[i].active) {
      sp.enemyBullets[i].x += sp.enemyBullets[i].dx;
      sp.enemyBullets[i].y += sp.enemyBullets[i].dy;
      if (sp.enemyBullets[i].y > SCREEN_HEIGHT + 2)
        sp.enemyBullets[i].active = false;
    }
  }

  // Update enemies
  for (int i = 0; i < SP_MAX_ENEMIES; i++) {
    if (!sp.enemies[i].active) continue;

    if (sp.enemies[i].type == 0) {
      // Fighter: straight down.
      sp.enemies[i].y += sp.enemies[i].dy;
      if (sp.enemies[i].y > SCREEN_HEIGHT + 10) {
        sp.enemies[i].active = false;
      }
    } else if (sp.enemies[i].type == 1) {
      // Bomber: enter from a side, then travel toward a random exposed idle position.
      unsigned long age = now - sp.enemies[i].spawnTime;
      if (age < 7000) {
        // Move at the bomber's original slow speed until the chosen target is reached.
        float tx = sp.enemies[i].targetX;
        float ty = sp.enemies[i].targetY;
        float vx = tx - sp.enemies[i].x;
        float vy = ty - sp.enemies[i].y;
        float dist = sqrt(vx * vx + vy * vy);
        if (dist > 0.5) {
          float step = 0.30;
          sp.enemies[i].x += (vx / dist) * step;
          sp.enemies[i].y += (vy / dist) * step;
        }
      } else {
        float phase = sp.enemies[i].idlePhase + (now / 1000.0);
        sp.enemies[i].x += sin(phase) * 0.22;
        sp.enemies[i].y += cos(phase * 0.7) * 0.08;
        if (sp.enemies[i].x < 1) sp.enemies[i].x = 1;
        if (sp.enemies[i].x > SCREEN_WIDTH - 12) sp.enemies[i].x = SCREEN_WIDTH - 12;
        if (sp.enemies[i].y < 5) sp.enemies[i].y = 5;
        if (sp.enemies[i].y > 38) sp.enemies[i].y = 38;
      }

      // Bomber fires randomly every 2.5-4.5 seconds.
      if (age > 1000 && (now - sp.enemies[i].lastShot) >= sp.enemies[i].shotInterval) {
        sp.enemies[i].shotInterval = random(2500, 4501);
        sp.enemies[i].lastShot = now;
        spawnEnemyBullet(sp.enemies[i].x + 5, sp.enemies[i].y + 9);
        playToneAsync(600, 30);
      }

      // Bomber eventually leaves upward.
      if (age > 35000) {
        sp.enemies[i].y -= 0.5;
        if (sp.enemies[i].y < -12) {
          sp.enemies[i].active = false;
          sp.activeBombers--;
        }
      }
    } else {
      // Scout: horizontal entry -> one moderate circle -> horizontal pass -> another circle -> exit.
      float speed = 0.30;
      int side = (sp.enemies[i].dx >= 0) ? 1 : -1;
      if (sp.enemies[i].movePhase == 0) {
        sp.enemies[i].x += sp.enemies[i].dx;
        sp.enemies[i].straightRemaining -= speed;
        sp.enemies[i].spriteDir = (side > 0) ? 1 : 3;
        if (sp.enemies[i].straightRemaining <= 0) {
          // Start the circle exactly at the current position, so there is no teleport/jump.
          sp.enemies[i].circleRadius = random(7, 11);
          sp.enemies[i].circleStartAngle = random(0, 628) / 100.0;
          sp.enemies[i].circleAngle = sp.enemies[i].circleStartAngle;
          sp.enemies[i].circleCX = sp.enemies[i].x - cos(sp.enemies[i].circleAngle) * sp.enemies[i].circleRadius;
          sp.enemies[i].circleCY = sp.enemies[i].y - sin(sp.enemies[i].circleAngle) * sp.enemies[i].circleRadius;
          sp.enemies[i].movePhase = 1;
        }
      } else if (sp.enemies[i].movePhase == 1 || sp.enemies[i].movePhase == 3) {
        float angleStep = speed / sp.enemies[i].circleRadius;
        float oldAngle = sp.enemies[i].circleAngle;
        sp.enemies[i].circleAngle += angleStep;
        sp.enemies[i].x = sp.enemies[i].circleCX + cos(sp.enemies[i].circleAngle) * sp.enemies[i].circleRadius;
        sp.enemies[i].y = sp.enemies[i].circleCY + sin(sp.enemies[i].circleAngle) * sp.enemies[i].circleRadius;

        // Face tangent to the circle.
        float tx = -sin(oldAngle);
        float ty = cos(oldAngle);
        if (fabs(tx) > fabs(ty)) sp.enemies[i].spriteDir = (tx > 0) ? 1 : 3;
        else sp.enemies[i].spriteDir = (ty > 0) ? 2 : 0;

        if (sp.enemies[i].circleAngle - sp.enemies[i].circleStartAngle >= TWO_PI) {
          sp.enemies[i].circlesDone++;
          if (sp.enemies[i].movePhase == 1) {
            sp.enemies[i].movePhase = 2;
            sp.enemies[i].straightRemaining = random(14, 32);
          } else {
            sp.enemies[i].movePhase = 4;
          }
        }
      } else if (sp.enemies[i].movePhase == 2) {
        sp.enemies[i].x += side * speed;
        sp.enemies[i].straightRemaining -= speed;
        sp.enemies[i].spriteDir = (side > 0) ? 1 : 3;
        if (sp.enemies[i].straightRemaining <= 0) {
          // Randomize the second circle while keeping the current point continuous.
          sp.enemies[i].circleRadius = random(7, 11);
          sp.enemies[i].circleStartAngle = random(0, 628) / 100.0;
          sp.enemies[i].circleAngle = sp.enemies[i].circleStartAngle;
          sp.enemies[i].circleCX = sp.enemies[i].x - cos(sp.enemies[i].circleAngle) * sp.enemies[i].circleRadius;
          sp.enemies[i].circleCY = sp.enemies[i].y - sin(sp.enemies[i].circleAngle) * sp.enemies[i].circleRadius;
          sp.enemies[i].movePhase = 3;
        }
      } else if (sp.enemies[i].movePhase == 4) {
        sp.enemies[i].x += side * speed;
        sp.enemies[i].spriteDir = (side > 0) ? 1 : 3;
        if (sp.enemies[i].x < -12 || sp.enemies[i].x > SCREEN_WIDTH + 12) {
          sp.enemies[i].active = false;
        }
      }

      // Random shooting like the bomber, but with a shorter interval.
      unsigned long age = now - sp.enemies[i].spawnTime;
      if (age > 900 && (now - sp.enemies[i].lastShot) >= sp.enemies[i].shotInterval) {
        sp.enemies[i].shotInterval = random(2600, 4301);
        sp.enemies[i].lastShot = now;
        spawnEnemyBullet(sp.enemies[i].x + 4, sp.enemies[i].y + 8);
        playToneAsync(650, 25);
      }
    }
  }

  // Update power-ups (falling slowly)
  for (int i = 0; i < SP_MAX_POWERUPS; i++) {
    if (sp.powerups[i].active) {
      sp.powerups[i].y += 0.5;
      if (sp.powerups[i].y > SCREEN_HEIGHT + 5)
        sp.powerups[i].active = false;
    }
  }

  // Update explosions
  for (int i = 0; i < SP_MAX_EXPLOSIONS; i++) {
    if (sp.explosions[i].active) {
      sp.explosions[i].frame++;
      if (sp.explosions[i].frame > 8) sp.explosions[i].active = false;
    }
  }

  // --- Collision: player bullets vs enemies ---
  for (int b = 0; b < SP_MAX_BULLETS; b++) {
    if (!sp.bullets[b].active) continue;
    for (int e = 0; e < SP_MAX_ENEMIES; e++) {
      if (!sp.enemies[e].active) continue;
      int ew = (sp.enemies[e].type == 0) ? 7 : (sp.enemies[e].type == 1 ? 11 : 9);
      int eh = (sp.enemies[e].type == 0) ? 6 : (sp.enemies[e].type == 1 ? 9 : 9);
      if (sp.bullets[b].x >= sp.enemies[e].x && sp.bullets[b].x <= sp.enemies[e].x + ew &&
          sp.bullets[b].y >= sp.enemies[e].y && sp.bullets[b].y <= sp.enemies[e].y + eh) {
        sp.bullets[b].active = false;
        sp.enemies[e].hp--;
        if (sp.enemies[e].hp <= 0) {
          spawnExplosion(sp.enemies[e].x, sp.enemies[e].y);
          spawnPowerUp(sp.enemies[e].x, sp.enemies[e].y);
          sp.score++;
          sp.enemiesKilledThisWave++;
          if (sp.enemies[e].type == 1) sp.activeBombers--;
          sp.enemies[e].active = false;
          playHit();
        } else {
          playToneAsync(1200, 15); // hit but not killed
        }
        break;
      }
    }
  }

  // --- Collision: enemies vs player ---
  if (!sp.invincible && !sp.gameOver) {
    for (int e = 0; e < SP_MAX_ENEMIES; e++) {
      if (!sp.enemies[e].active) continue;
      int ew = (sp.enemies[e].type == 0) ? 7 : (sp.enemies[e].type == 1 ? 11 : 9);
      int eh = (sp.enemies[e].type == 0) ? 6 : (sp.enemies[e].type == 1 ? 9 : 9);
      // Simple AABB
      if (sp.playerX + SP_PLAYER_W > sp.enemies[e].x && sp.playerX < sp.enemies[e].x + ew &&
          SP_PLAYER_Y + SP_PLAYER_H > sp.enemies[e].y && SP_PLAYER_Y < sp.enemies[e].y + eh) {
        if (sp.shieldActive && millis() < sp.shieldEnd) {
          // Shield absorbs hit, destroy enemy
          spawnExplosion(sp.enemies[e].x, sp.enemies[e].y);
          sp.score++;
          sp.enemiesKilledThisWave++;
          if (sp.enemies[e].type == 1) sp.activeBombers--;
          sp.enemies[e].active = false;
          playHit();
        } else {
          // Player dies
          playerDeath();
        }
      }
    }
  }

  // --- Collision: enemy bullets vs player ---
  if (!sp.invincible && !sp.gameOver) {
    for (int b = 0; b < SP_MAX_ENEMY_BULLETS; b++) {
      if (!sp.enemyBullets[b].active) continue;
      if (sp.enemyBullets[b].x >= sp.playerX && sp.enemyBullets[b].x <= sp.playerX + SP_PLAYER_W &&
          sp.enemyBullets[b].y >= SP_PLAYER_Y && sp.enemyBullets[b].y <= SP_PLAYER_Y + SP_PLAYER_H) {
        sp.enemyBullets[b].active = false;
        if (sp.shieldActive && millis() < sp.shieldEnd) {
          // Shield absorbs
          playToneAsync(1800, 20);
        } else {
          playerDeath();
        }
      }
    }
  }

  // --- Collision: player vs power-ups ---
  for (int i = 0; i < SP_MAX_POWERUPS; i++) {
    if (!sp.powerups[i].active) continue;
    if (sp.powerups[i].x + 5 > sp.playerX && sp.powerups[i].x < sp.playerX + SP_PLAYER_W &&
        sp.powerups[i].y + 5 > SP_PLAYER_Y && sp.powerups[i].y < SP_PLAYER_Y + SP_PLAYER_H) {
      sp.powerups[i].active = false;
      switch (sp.powerups[i].type) {
        case 0: // Shield
          sp.shieldActive = true;
          sp.shieldEnd = now + 10000;
          break;
        case 1: // Tri-shot
          sp.triShotActive = true;
          sp.triShotEnd = now + 8000;
          break;
        case 2: // Bomb
          sp.bombCount++;
          if (sp.bombCount > 3) sp.bombCount = 3;
          break;
      }
      playPowerUp();
    }
  }

  // --- Shield timeout ---
  if (sp.shieldActive && now >= sp.shieldEnd) {
    sp.shieldActive = false;
  }

  // --- Tri-shot timeout ---
  if (sp.triShotActive && now >= sp.triShotEnd) {
    sp.triShotActive = false;
  }

  // --- Invincibility timeout ---
  if (sp.invincible && now >= sp.invincibleEnd) {
    sp.invincible = false;
  }

  // --- Wave / spawn logic ---
  if (!sp.gameOver) {
    // Spawn enemies at intervals
    if (sp.enemiesSpawned < sp.enemiesPerWave) {
      if (now >= sp.spawnUnlockTime && now - sp.lastEnemySpawn > (unsigned long)sp.spawnInterval) {
        spawnEnemy();
        sp.lastEnemySpawn = now;
      }
    } else {
      // Check if wave is complete (all enemies spawned and killed/gone)
      bool anyActive = false;
      for (int i = 0; i < SP_MAX_ENEMIES; i++) {
        if (sp.enemies[i].active) { anyActive = true; break; }
      }
      if (!anyActive) {
        // Next wave!
        advanceWave();
      }
    }
  }
}

void playerDeath() {
  sp.lives--;
  spawnExplosion(sp.playerX, SP_PLAYER_Y);
  if (sp.lives <= 0) {
    sp.gameOver = true;
    playGameOver();
  } else {
    // Respawn with brief invincibility
    sp.invincible = true;
    sp.invincibleEnd = millis() + 2000;
    sp.playerX = SCREEN_WIDTH / 2 - SP_PLAYER_W / 2;
    sp.shieldActive = false;
    sp.triShotActive = false;
    playDeath();
  }
}

void advanceWave() {
  sp.wave++;
  sp.difficulty++;
  sp.enemiesSpawned = 0;
  sp.enemiesKilledThisWave = 0;
  sp.waveStartTime = millis();
  sp.spawnUnlockTime = sp.waveStartTime + 1800; // give player time to react before new enemies appear
  sp.lastEnemySpawn = sp.waveStartTime;
  sp.scoutsSpawnedThisWave = 0;

  // Increase enemies per wave with difficulty
  sp.enemiesPerWave = 5 + (sp.difficulty - 1) * 2;
  if (sp.enemiesPerWave > 30) sp.enemiesPerWave = 30;

  // Slightly faster spawn rate
  sp.spawnInterval = 1200 - (sp.difficulty * 50);
  if (sp.spawnInterval < 400) sp.spawnInterval = 400;

  // Brief wave announcement will be shown in draw
  playToneAsync(1047, 80);
}

// --- Draw Game ---
void drawSkyPatrol() {
  display.clearDisplay();

  // Background stars
  drawStars();
  updateStars();

  // Draw power-ups
  for (int i = 0; i < SP_MAX_POWERUPS; i++) {
    if (!sp.powerups[i].active) continue;
    int px = (int)sp.powerups[i].x;
    int py = (int)sp.powerups[i].y;
    // Draw as small labeled box
    display.drawRect(px, py, 7, 7, SH110X_WHITE);
    switch (sp.powerups[i].type) {
      case 0: // Shield - 'S'
        display.setCursor(px + 1, py);
        display.setTextSize(1);
        display.print("S");
        break;
      case 1: // Tri-shot - 'T'
        display.setCursor(px + 1, py);
        display.setTextSize(1);
        display.print("T");
        break;
      case 2: // Bomb - 'B'
        display.setCursor(px + 1, py);
        display.setTextSize(1);
        display.print("B");
        break;
    }
  }

  // Draw enemies
  for (int i = 0; i < SP_MAX_ENEMIES; i++) {
    if (!sp.enemies[i].active) continue;
    if (sp.enemies[i].type == 0) {
      drawFighter((int)sp.enemies[i].x, (int)sp.enemies[i].y);
    } else if (sp.enemies[i].type == 1) {
      drawBomber((int)sp.enemies[i].x, (int)sp.enemies[i].y);
    } else {
      drawScout((int)sp.enemies[i].x, (int)sp.enemies[i].y, sp.enemies[i].spriteDir);
    }
  }

  // Draw player bullets
  for (int i = 0; i < SP_MAX_BULLETS; i++) {
    if (sp.bullets[i].active) {
      display.fillRect((int)sp.bullets[i].x, (int)sp.bullets[i].y, 1, 3, SH110X_WHITE);
    }
  }

  // Draw enemy bullets
  for (int i = 0; i < SP_MAX_ENEMY_BULLETS; i++) {
    if (sp.enemyBullets[i].active) {
      // Enemy bullets are slightly different - small dot
      display.fillRect((int)sp.enemyBullets[i].x, (int)sp.enemyBullets[i].y, 2, 2, SH110X_WHITE);
    }
  }

  // Draw player
  drawPlayer();

  // Draw explosions
  for (int i = 0; i < SP_MAX_EXPLOSIONS; i++) {
    if (!sp.explosions[i].active) continue;
    int r = sp.explosions[i].frame;
    if (r < 4) {
      display.drawCircle((int)sp.explosions[i].x + 3, (int)sp.explosions[i].y + 3, r + 1, SH110X_WHITE);
    } else {
      display.drawCircle((int)sp.explosions[i].x + 3, (int)sp.explosions[i].y + 3, 8 - r, SH110X_WHITE);
    }
  }

  // --- HUD ---
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // Score top-left
  display.setCursor(0, 0);
  display.print(sp.score);

  // Wave top-center
  display.setCursor(50, 0);
  display.print("W");
  display.print(sp.wave);

  // Lives top-right (small hearts/dots)
  for (int i = 0; i < sp.lives; i++) {
    display.fillRect(112 + i * 6, 1, 4, 4, SH110X_WHITE);
  }

  // Active power-up indicators at top
  if (sp.shieldActive) {
    display.setCursor(25, 0);
    display.print("SH");
  }
  if (sp.triShotActive) {
    display.setCursor(37, 0);
    display.print("TR");
  }
  if (sp.bombCount > 0) {
    display.setCursor(100, 0);
    display.print("B");
    display.print(sp.bombCount);
  }

  // Wave announcement
  unsigned long waveAge = millis() - sp.waveStartTime;
  if (waveAge < 1800) {
    display.setTextSize(1);
    display.setCursor(36, 28);
    display.print("WAVE ");
    display.print(sp.wave);
  }

  // Game Over screen
  if (sp.gameOver) {
    display.fillRect(14, 18, 100, 30, SH110X_BLACK);
    display.drawRect(14, 18, 100, 30, SH110X_WHITE);
    display.setTextSize(1);
    display.setCursor(28, 22);
    display.print("GAME OVER!");
    display.setCursor(28, 34);
    display.print("Score: ");
    display.print(sp.score);
    display.setCursor(22, 44);
    display.print("[ENTER] Retry");
  }

  display.display();
}

// ============================================================
// SETUP & MAIN LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  randomSeed((unsigned long)micros());
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(TOUCH_PIN, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);

  // Initialize button debouncing from the actual idle (HIGH) state.
  btnLeft.lastRaw = false;
  btnRight.lastRaw = false;
  btnEnter.lastRaw = false;

  if (BATTERY_PIN >= 0) pinMode(BATTERY_PIN, INPUT);

  bool displayOK = display.begin(0x3C, true);
  if (!displayOK) {
    displayOK = display.begin(0x3D, true);
  }

  if (!displayOK) {
    Serial.println("ERROR: OLED not found!");
    while (true) {
      tone(BUZZER_PIN, 220, 200);
      delay(400);
    }
  }
  display.setTextColor(SH110X_WHITE);

  playBootSound();
  playBootAnimation();

  initMochi();
  currentState = STATE_MOCHI;
  Serial.println("Dasai-Mochi initialized - Mochi mode.");
}

void loop() {
  unsigned long now = millis();

  // Frame rate cap (~30 FPS)
  static unsigned long lastFrame = 0;
  if (now - lastFrame < 33) return;
  lastFrame = now;

  // Read all buttons
  updateAllButtons();

  // Long touch opens the separate GAME menu from Mochi,
  // and returns from a game to the normal Mochi face.
  if (checkTouchBack()) {
    if (currentState == STATE_MOCHI) {
      currentState = STATE_GAME_MENU;
      menuSelection = 0;
      playMenuSelect();
      return;
    } else if (currentState == STATE_GAME_MENU || currentState == STATE_GAME_SKY_PATROL) {
      currentState = STATE_MOCHI;
      initMochi();
      playMenuSelect();
      return;
    }
  }

  // State machine
  switch (currentState) {
    case STATE_GAME_MENU:
      updateGameMenu();
      break;
    case STATE_MOCHI:
      updateMochi();
      break;
    case STATE_GAME_SKY_PATROL:
      handleSkyPatrolInput();
      updateSkyPatrol();
      drawSkyPatrol();
      break;
  }
}
