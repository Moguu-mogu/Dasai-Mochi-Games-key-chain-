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
void playTone(int frequency, int durationMs);
void playToneAsync(int frequency, int durationMs);
void playBootSound();
void playBootAnimation();

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
  bool raw = digitalRead(pin);
  unsigned long now = millis();
  if (raw != b.lastRaw) {
    b.lastChange = now;
    b.lastRaw = raw;
  }
  bool stable = b.pressed;
  if ((now - b.lastChange) > 30) {
    stable = raw;
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
  STATE_MENU,
  STATE_MOCHI,
  STATE_GAME_SKY_PATROL,
  // Future: STATE_GAME_2, STATE_GAME_3, etc.
};

AppState currentState = STATE_MENU;
int menuSelection = 0;

// --- Menu Feature Entries ---
#define NUM_FEATURES 2
const char* featureNames[NUM_FEATURES] = {
  "Mochi Pet",
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
// MAIN MENU
// ============================================================
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // Title bar with line
  display.setCursor(24, 2);
  display.print("DASAI MOCHI");
  display.drawLine(0, 12, 127, 12, SH110X_WHITE);

  // Feature list
  for (int i = 0; i < NUM_FEATURES; i++) {
    int y = 16 + i * 12;
    if (i == menuSelection) {
      // Selection indicator
      display.setCursor(2, y);
      display.print(">");
      // Highlight bar
      display.fillRect(10, y - 1, 118, 11, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
      display.setCursor(14, y);
      display.print(featureNames[i]);
      display.setTextColor(SH110X_WHITE);
    } else {
      display.setCursor(14, y);
      display.print(featureNames[i]);
    }
  }

  // Bottom bar - navigation hints
  display.drawLine(0, 53, 127, 53, SH110X_WHITE);

  // Left arrow + "BACK"
  display.setCursor(2, 56);
  display.print("<");
  display.setCursor(10, 56);
  display.print("BACK");

  // Right side: "ENTER" + right arrow
  display.setCursor(85, 56);
  display.print("ENTER");
  display.setCursor(120, 56);
  display.print(">");

  display.display();
}

bool menuConfirmActive = false;

void updateMenu() {
  if (btnLeft.justPressed) {
    menuSelection--;
    if (menuSelection < 0) menuSelection = NUM_FEATURES - 1;
    playMenuMove();
  }
  if (btnRight.justPressed) {
    menuSelection++;
    if (menuSelection >= NUM_FEATURES) menuSelection = 0;
    playMenuMove();
  }
  if (btnEnter.justPressed) {
    playMenuSelect();
    // Enter the selected feature
    switch (menuSelection) {
      case 0: currentState = STATE_MOCHI; initMochi(); break;
      case 1: currentState = STATE_GAME_SKY_PATROL; initSkyPatrol(); break;
    }
  }
  drawMenu();
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
  } else if (mood == MOOD_SLEEPY || mood == MOOD_LOW_BATTERY) {
    display.fillRect(ix, iy, iw, ih / 2 + (mood == MOOD_LOW_BATTERY ? 4 : 2), SH110X_BLACK);
  } else if (mood == MOOD_SUSPICIOUS) {
    if (isLeft) display.fillRect(ix, iy, iw, ih / 2 - 2, SH110X_BLACK);
    else display.fillRect(ix, iy + ih - 8, iw, 8, SH110X_BLACK);
  }
}

void drawUltraProEye(Eye& e, bool isLeft, int mood) {
  int ix = (int)e.x, iy = (int)e.y, iw = (int)e.w, ih = (int)e.h;
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
      case MOOD_SLEEPY: leftEye.targetW = 38; leftEye.targetH = 30; rightEye.targetW = 38; rightEye.targetH = 30; break;
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

void handleMochiTouch() {
  // Mochi uses the 3 buttons for interaction while in pet mode
  if (btnLeft.justPressed) {
    currentMood--;
    if (currentMood < 0) currentMood = MOOD_LOW_BATTERY;
    lastSaccade = 0;
    playToneAsync(1500, 40);
  }
  if (btnRight.justPressed) {
    currentMood++;
    if (currentMood > MOOD_LOW_BATTERY) currentMood = 0;
    lastSaccade = 0;
    playToneAsync(1500, 40);
  }
  if (btnEnter.justPressed) {
    // Random mood on enter
    currentMood = random(0, 10);
    lastSaccade = 0;
    playMenuSelect();
  }
}

void updateMochi() {
  handleMochiTouch();
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

  // Show "Hold touch to exit" hint briefly
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(16, 56);
  display.print("Hold touch=Menu");
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

// Fighter sprite (5x6) - small enemy plane
const unsigned char spr_fighter[] PROGMEM = {
  0b01010000,  //  * *
  0b01110000,  //  ***
  0b11111000,  //  *****
  0b11111000,  //  *****
  0b01110000,  //  ***
  0b00100000   //   *
};

// Bomber sprite (11x9) - twin-engine bomber, bigger
const unsigned char spr_bomber[] PROGMEM = {
  0b00010100000, // Stored as 2 bytes per row
  0b00111110000,
  0b01111111000,
  0b11111111100,
  0b11101110100, // twin engine shape (not used as bitmap, drawn procedurally)
  0b11111111100,
  0b01111111000,
  0b00101010000,
  0b00101010000
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
  int type; // 0 = fighter, 1 = bomber
  bool active;
  unsigned long spawnTime;
  unsigned long lastShot; // bomber only
  float idlePhase;        // bomber idle movement phase
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
  int spawnInterval; // ms between enemy spawns

  // Bomber tracking
  int activeBombers;

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
  sp.lastFrame = millis();
  sp.lastShot = 0;
  sp.bombCount = 0;
  sp.activeBombers = 0;

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
    // Flicker near end
    if (sp.shieldEnd - millis() < 3000) {
      if ((millis() / 200) % 2 == 0)
        display.drawCircle(px + 3, py + 3, 6, SH110X_WHITE);
    }
  }
}

// --- Draw Fighter Enemy ---
void drawFighter(int x, int y) {
  // Small enemy plane pointing downward
  display.drawPixel(x + 2, y, SH110X_WHITE);      // nose
  display.fillRect(x + 1, y + 1, 3, 2, SH110X_WHITE); // body
  display.fillRect(x, y + 3, 5, 2, SH110X_WHITE);  // wings
  display.drawPixel(x + 1, y + 5, SH110X_WHITE);   // tail
  display.drawPixel(x + 3, y + 5, SH110X_WHITE);   // tail
}

// --- Draw Bomber Enemy ---
void drawBomber(int x, int y) {
  // Twin-engine bomber - bigger, facing upward (sprite faces up)
  // Center fuselage
  display.fillRect(x + 4, y, 3, 9, SH110X_WHITE);
  // Wings
  display.fillRect(x, y + 3, 11, 2, SH110X_WHITE);
  // Left engine
  display.fillRect(x + 1, y + 2, 2, 4, SH110X_WHITE);
  // Right engine
  display.fillRect(x + 8, y + 2, 2, 4, SH110X_WHITE);
  // Tail
  display.fillRect(x + 3, y + 7, 5, 2, SH110X_WHITE);
  // Cockpit dot
  display.drawPixel(x + 5, y + 1, SH110X_BLACK);

  // HP indicator - small dots above
  // (visual cue that it's tougher)
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
  if (random(0, 100) > 25) return;
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

      // Bombers only appear at difficulty > 6, max 2 active
      if (sp.difficulty > 6 && sp.activeBombers < 2) {
        // 20% chance for bomber at eligible difficulty
        if (random(0, 100) < 20) {
          spawnBomber = true;
        }
      }

      if (spawnBomber) {
        // Bomber
        sp.enemies[i].type = 1;
        sp.enemies[i].hp = 5;
        // Appear from random side
        bool fromLeft = random(0, 2) == 0;
        sp.enemies[i].x = fromLeft ? -11 : SCREEN_WIDTH;
        sp.enemies[i].y = random(2, 20);
        sp.enemies[i].dx = fromLeft ? 0.3 : -0.3; // drift in
        sp.enemies[i].dy = 0;
        sp.enemies[i].idlePhase = random(0, 628) / 100.0; // random start phase
        sp.enemies[i].lastShot = millis();
        sp.activeBombers++;
      } else {
        // Fighter
        sp.enemies[i].type = 0;
        sp.enemies[i].hp = 1;
        sp.enemies[i].x = random(2, SCREEN_WIDTH - 7);
        sp.enemies[i].y = -8;
        sp.enemies[i].dx = 0;
        sp.enemies[i].dy = 0.5 + (sp.difficulty * 0.05); // slightly faster each difficulty
        if (sp.enemies[i].dy > 1.2) sp.enemies[i].dy = 1.2; // cap speed for balance
      }

      sp.enemies[i].active = true;
      sp.enemies[i].spawnTime = millis();
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

  // Fire: single press each shot
  if (btnEnter.justPressed) {
    unsigned long now = millis();
    if (now - sp.lastShot > 150) { // fire rate limiter
      sp.lastShot = now;

      if (sp.bombCount > 0) {
        // Check if we want to use bomb - long press Enter? No, the user said
        // single press = shoot. Bombs will be a separate mechanic:
        // We'll use bomb when both Left+Right are held and Enter is pressed
      }

      float bx = sp.playerX + SP_PLAYER_W / 2;
      float by = SP_PLAYER_Y - 2;

      if (sp.triShotActive && millis() < sp.triShotEnd) {
        // Tri-shot: fan of 3 bullets
        spawnBullet(bx, by, 0, -3);      // center
        spawnBullet(bx, by, -1, -2.8);   // left
        spawnBullet(bx, by, 1, -2.8);    // right
      } else {
        sp.triShotActive = false;
        spawnBullet(bx, by, 0, -3);      // single shot
      }
      playShoot();
    }
  }

  // Bomb: Hold both Left+Right and press Enter
  if (btnLeft.pressed && btnRight.pressed && btnEnter.justPressed && sp.bombCount > 0) {
    sp.bombCount--;
    // Clear all enemies on screen (don't auto-advance wave)
    for (int i = 0; i < SP_MAX_ENEMIES; i++) {
      if (sp.enemies[i].active) {
        spawnExplosion(sp.enemies[i].x, sp.enemies[i].y);
        sp.score++;
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
      // Fighter: straight down
      sp.enemies[i].y += sp.enemies[i].dy;
      if (sp.enemies[i].y > SCREEN_HEIGHT + 10) {
        sp.enemies[i].active = false;
      }
    } else {
      // Bomber: idle movement, drift into position then hover
      unsigned long age = now - sp.enemies[i].spawnTime;

      if (age < 2000) {
        // Drifting into position
        sp.enemies[i].x += sp.enemies[i].dx;
      } else {
        // Idle hovering: gentle sine wave movement
        sp.enemies[i].dx = 0;
        float phase = sp.enemies[i].idlePhase + (now / 1000.0);
        sp.enemies[i].x += sin(phase) * 0.3;
        // Keep in bounds
        if (sp.enemies[i].x < 0) sp.enemies[i].x = 0;
        if (sp.enemies[i].x > SCREEN_WIDTH - 11) sp.enemies[i].x = SCREEN_WIDTH - 11;
      }

      // Bomber fires every 4 seconds
      if (age > 1000 && (now - sp.enemies[i].lastShot) >= 4000) {
        sp.enemies[i].lastShot = now;
        spawnEnemyBullet(sp.enemies[i].x + 5, sp.enemies[i].y + 9);
        playToneAsync(600, 30);
      }

      // Bomber disappears after 35 seconds (leaves forward/upward)
      if (age > 35000) {
        // Start leaving - move upward
        sp.enemies[i].dy = -0.5;
        sp.enemies[i].y += sp.enemies[i].dy;
        if (sp.enemies[i].y < -12) {
          sp.enemies[i].active = false;
          sp.activeBombers--;
        }
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
      int ew = (sp.enemies[e].type == 0) ? 5 : 11;
      int eh = (sp.enemies[e].type == 0) ? 6 : 9;
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
      int ew = (sp.enemies[e].type == 0) ? 5 : 11;
      int eh = (sp.enemies[e].type == 0) ? 6 : 9;
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
      if (now - sp.lastEnemySpawn > (unsigned long)sp.spawnInterval) {
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
    } else {
      drawBomber((int)sp.enemies[i].x, (int)sp.enemies[i].y);
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
  if (waveAge < 1500) {
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
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(TOUCH_PIN, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_LEFT, INPUT_PULLDOWN);
  pinMode(BTN_RIGHT, INPUT_PULLDOWN);
  pinMode(BTN_ENTER, INPUT_PULLDOWN);

  if (BATTERY_PIN >= 0) pinMode(BATTERY_PIN, INPUT);

  if (!display.begin(0x3C, true) && !display.begin(0x3D, true)) {
    Serial.println("ERROR: OLED not found!");
    while (true) { tone(BUZZER_PIN, 220, 200); delay(400); }
  }
  display.setTextColor(SH110X_WHITE);

  playBootSound();
  playBootAnimation();

  currentState = STATE_MENU;
  Serial.println("Dasai-Mochi initialized.");
}

void loop() {
  unsigned long now = millis();

  // Frame rate cap (~30 FPS)
  static unsigned long lastFrame = 0;
  if (now - lastFrame < 33) return;
  lastFrame = now;

  // Read all buttons
  updateAllButtons();

  // Check touch-back for returning to menu (from any state except menu)
  if (currentState != STATE_MENU) {
    if (checkTouchBack()) {
      currentState = STATE_MENU;
      playMenuSelect();
      return;
    }
  }

  // State machine
  switch (currentState) {
    case STATE_MENU:
      updateMenu();
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