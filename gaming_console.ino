/*************************************************************************************************** * IFM LABZ + SECRET CODE
/***************************************************************************************************
 * IFM LABZ + SECRET CODE GAME (cleaned)
 * - Removed SD/Image/Reaction/Joystick-demo menu code
 * - Kept game logic and joystick behavior exactly as in your current working version
 * - Runs directly in game mode
 ***************************************************************************************************/

#include "SPI.h"
#include "Adafruit_ST7735.h"
#include "pca9557_cdd.h"
#include "LcdUtils.h"

/***************************************************************************************************
 *                                     DEFINES — Timing
 ***************************************************************************************************/
#define CYCLE_TIME_10MS 10
#define CYCLE_TIME_5MS 5
#define DEBOUNCE_MS 120

/***************************************************************************************************
 *                                     DEFINES — Push Buttons
 ***************************************************************************************************/
#define SW1_PIN 3
#define SW2_PIN 2
#define SW3_PIN 4
#define SW4_PIN 9

/***************************************************************************************************
 *                                     DEFINES — Joystick
 ***************************************************************************************************/
#define JOY_VRX_PIN A0
#define JOY_VRY_PIN A1
#define JOY_BUTTON A2

/***************************************************************************************************
 *                                     DEFINES — LCD Pins
 ***************************************************************************************************/
#if defined(D5)
#define LCD_RST_PIN D5
#else
#define LCD_RST_PIN 5
#endif

#if defined(D6)
#define LCD_CS_PIN D6
#else
#define LCD_CS_PIN 6
#endif

#if defined(D7)
#define LCD_DC_PIN D7
#else
#define LCD_DC_PIN 7
#endif

#if defined(D10)
#define LCD_BCKL_PIN D10
#else
#define LCD_BCKL_PIN 10
#endif

/***************************************************************************************************
 *                                     DEFINES — PCA9557
 ***************************************************************************************************/
#define PCA_ADDRESS 25

/***************************************************************************************************
 *                                     SECRET GAME STATES
 ***************************************************************************************************/
#define SG_IDLE 0U
#define SG_PICK_SECRET 1U
#define SG_PICK_GUESS 2U
#define SG_SHOW_RESULT 3U
#define SG_WIN 4U
#define SG_SHOP 5U

/***************************************************************************************************
 *                                     DEFINES — Display Geometry
 ***************************************************************************************************/
#define ST77XX_GRAY 0x7BEF

/**************************************************************************************************
                                      DEFINES- BUZZER
****************************************************************************************************/

#define BUZZER_PIN 8 

/***************************************************************************************************
 *                                     GLOBALS
 ***************************************************************************************************/
unsigned long time0 = 0;

volatile bool B1Pressed = false;
volatile bool B2Pressed = false;
volatile bool B3Pressed = false;
volatile bool B4Pressed = false;

volatile unsigned long lastISR_SW1 = 0;
volatile unsigned long lastISR_SW2 = 0;
volatile unsigned long lastISR_SW3 = 0;
volatile unsigned long lastISR_SW4 = 0;

int joyCenterX = 2048;
int joyCenterY = 2048;

/* LCD obj */
Adafruit_ST7735 lcd = Adafruit_ST7735(LCD_CS_PIN, LCD_DC_PIN, -1);

/***************************************************************************************************
 *                              SECRET CODE GAME DATA
 ***************************************************************************************************/
uint8_t secretGameState = SG_IDLE;
uint8_t secretCode[3] = {0, 0, 0};
uint8_t guessCode[3] = {0, 0, 0};
uint8_t selectedIdx = 0;
uint8_t attemptCount = 0;
uint8_t maxAttempts = 8;
uint8_t lastExact = 0;
uint8_t lastPartial = 0;
bool secretLocked = false;
unsigned long lastJoyMoveMs = 0;

long totalScore = 0;
uint8_t prevExactForCombo = 0;

uint8_t bestExactThisRound = 0;

uint8_t hintInventory = 0;
uint8_t jamInventory = 0;

int8_t  jamDirections[3]  = {0, 0, 0}; // 1=secret higher(^), -1=lower(v), 0=exact(=)
bool    jamResultActive   = false;

bool hintUsedThisRound = false;
bool jamUsedThisRound = false;
bool jamArmed = false;

/***************************************************************************************************
 *                                     PROTOTYPES
 ***************************************************************************************************/
void IRAM_ATTR ISR_SW1(void);
void IRAM_ATTR ISR_SW2(void);
void IRAM_ATTR ISR_SW3(void);
void IRAM_ATTR ISR_SW4(void);

void Task1_10ms(void);
void Task2_5ms(void);

static bool LCD_init(void);
static bool SERIAL_init(void);

static void buttonReactSw1(void);
static void buttonReactSw2(void);
static void buttonReactSw3(void);
static void buttonReactSw4(void);
static void processButtons(void);

static void playWinMelody(void);
static void playLoseMelody(void);

/* secret game helpers */
static void SG_enter(void);
static void SG_resetRound(void);
static void SG_drawPickSecret(void);
static void SG_drawPickGuess(void);
static void SG_drawResult(void);
static void SG_drawWin(void);
static void SG_showHintBlink(uint8_t pos, uint8_t val);
static void SG_handleJoystick(void);
static void SG_applyGuess(void);
static void SG_evalGuess(uint8_t guess[3], uint8_t secret[3], uint8_t* exact, uint8_t* partial);
static void SG_updatePcaLeds(uint8_t value);


static void SG_drawShop(void);
static void SG_awardPointsAfterGuess(uint8_t exactNow);

/***************************************************************************************************
 *                                     SETUP
 ***************************************************************************************************/
void setup() {
  SERIAL_init();

  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);
  pinMode(SW3_PIN, INPUT);
  pinMode(SW4_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  if (LCD_init()) {
    LcdUtils_init(&lcd);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    while (1) ;
  }

  if (PCA_initialize(PCA_ADDRESS)) {
    Serial.println("PCA OK");
  } else {
    Serial.println("PCA FAILED");
  }

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, LOW);    // magenta
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, LOW);

  analogReadResolution(12);
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 16; i++) {
    sumX += analogRead(JOY_VRY_PIN);
    sumY += analogRead(JOY_VRX_PIN);
    delay(5);
  }
  joyCenterX = sumX / 16;
  joyCenterY = sumY / 16;

  randomSeed(analogRead(7));
  time0 = millis();

  digitalWrite(LCD_RST_PIN, HIGH);
  delay(50);
  digitalWrite(LCD_RST_PIN, LOW);
  delay(50);
  digitalWrite(LCD_RST_PIN, HIGH);
  delay(150);
  SPI.begin();
  lcd.initR(INITR_TDO128x96);
  lcd.setSPISpeed(16000000UL);
  lcd.setRotation(0);
  lcd.fillScreen(ST77XX_BLACK);
  lcd.setTextWrap(false);
  lcd.setTextSize(1);
  LcdUtils_init(&lcd);

  attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW2_PIN), ISR_SW2, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW3_PIN), ISR_SW3, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW4_PIN), ISR_SW4, FALLING);

  SG_enter();
  Serial.println("Secret game ready");
}

/***************************************************************************************************
 *                                     LOOP
 ***************************************************************************************************/
void loop() {
  static unsigned long previousMillis5ms = 0;
  static unsigned long previousMillis10ms = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis10ms >= CYCLE_TIME_10MS) {
    Task1_10ms();
    previousMillis10ms = currentMillis;
  }

  if (currentMillis - previousMillis5ms >= CYCLE_TIME_5MS) {
    Task2_5ms();
    previousMillis5ms = currentMillis;
  }
}

/***************************************************************************************************
 *                                     TASKS
 ***************************************************************************************************/
void Task1_10ms(void) {
  processButtons();
}

void Task2_5ms(void) {
  SG_handleJoystick();
}

/***************************************************************************************************
 *                                     ISR
 ***************************************************************************************************/
void IRAM_ATTR ISR_SW1() {
  unsigned long now = millis();
  if (now - lastISR_SW1 >= DEBOUNCE_MS) {
    lastISR_SW1 = now;
    B1Pressed = true;
  }
}

void IRAM_ATTR ISR_SW2() {
  unsigned long now = millis();
  if (now - lastISR_SW2 >= DEBOUNCE_MS) {
    lastISR_SW2 = now;
    B2Pressed = true;
  }
}

void IRAM_ATTR ISR_SW3() {
  unsigned long now = millis();
  if (now - lastISR_SW3 >= DEBOUNCE_MS) {
    lastISR_SW3 = now;
    B3Pressed = true;
  }
}

void IRAM_ATTR ISR_SW4() {
  unsigned long now = millis();
  if (now - lastISR_SW4 >= DEBOUNCE_MS) {
    lastISR_SW4 = now;
    B4Pressed = true;
  }
}

/***************************************************************************************************
 *                                     BUTTON HANDLERS
 ***************************************************************************************************/
static void buttonReactSw1(void) {
  if (secretGameState == SG_SHOP) {
    if (totalScore >= 30) {
      totalScore -= 30;
      hintInventory++;
      SG_drawShop();
    }
    return;
  }
 if (secretGameState == SG_PICK_GUESS && hintInventory > 0 && !hintUsedThisRound) {
  hintInventory--;
  hintUsedThisRound = true;

  uint8_t pos = random(0, 3);
  uint8_t val = secretCode[pos];

  SG_showHintBlink(pos, val);

  Serial.print("Hint P");
  Serial.print((unsigned)(pos + 1));
  Serial.print("=");
  Serial.println((unsigned)val);
}
}

static void buttonReactSw2(void) {
  // reset round
  SG_resetRound();
}

static void buttonReactSw3(void) {
  // 1) Lock secret -> guess phase
  if (secretGameState == SG_PICK_SECRET) {
    secretLocked = true;
    secretGameState = SG_PICK_GUESS;
    selectedIdx = 0;
    SG_drawPickGuess();
    return;
  }

  // 2) Submit guess
  if (secretGameState == SG_PICK_GUESS) {
    SG_applyGuess();
    return;
  }

  // 3) From intermediate result screen:
  //    - if game ended (win/lose) -> SHOP
  //    - else continue guessing
  if (secretGameState == SG_SHOW_RESULT) {
    if (lastExact == 3 || attemptCount >= maxAttempts) {
      secretGameState = SG_SHOP;
      SG_drawShop();
    } else {
      secretGameState = SG_PICK_GUESS;
      SG_drawPickGuess();
    }
    return;
  }

  // 4) Win screen -> SHOP
  if (secretGameState == SG_WIN) {
    secretGameState = SG_SHOP;
    SG_drawShop();
    return;
  }

  // 5) Shop -> start new round (score stays)
  if (secretGameState == SG_SHOP) {
    SG_resetRound();
    return;
  }
}

static void buttonReactSw4(void) {
  // SHOP: buy jam
  if (secretGameState == SG_SHOP) {
    if (totalScore >= 50) {
      totalScore -= 50;
      jamInventory++;
      SG_drawShop();
    }
    return;
  }

  // IN-GAME: use jam (only in guess menu)
  if (secretGameState == SG_PICK_GUESS && jamInventory > 0 && !jamUsedThisRound) {
    jamInventory--;
    jamUsedThisRound = true;
    jamArmed = true;

    lcd.fillRect(0, 84, 128, 12, ST77XX_BLACK);
    LcdUtils_setCursor(0, 84);
    LcdUtils_printLine("JAM armed", RED, FONT_DEFAULT);

    Serial.println("JAM armed");
  }
}
static void processButtons(void) {
  bool Button1, Button2, Button3, Button4;

  noInterrupts();
  Button1 = B1Pressed; B1Pressed = false;
  Button2 = B2Pressed; B2Pressed = false;
  Button3 = B3Pressed; B3Pressed = false;
  Button4 = B4Pressed; B4Pressed = false;
  interrupts();

  if (Button1) buttonReactSw1();
  if (Button2) buttonReactSw2();
  if (Button3) buttonReactSw3();
  if (Button4) buttonReactSw4();
}

static void playWinMelody(void) {
  tone(BUZZER_PIN, 523, 80);
  delay(100);
  tone(BUZZER_PIN, 659, 80);
  delay(100);
  tone(BUZZER_PIN, 784, 80);
  delay(100);
  tone(BUZZER_PIN, 1047, 80);
  delay(100);
  tone(BUZZER_PIN, 1318, 80);
  delay(100);
  tone(BUZZER_PIN, 1568, 400);
  delay(450);
  noTone(BUZZER_PIN);
}


static void playLoseMelody(void) {
  tone(BUZZER_PIN, 392, 200);  
  delay(250);
  tone(BUZZER_PIN, 349, 200);  
  delay(250);
  tone(BUZZER_PIN, 311, 250);  
  delay(300);
  tone(BUZZER_PIN, 261, 500);  
  delay(550);
  noTone(BUZZER_PIN);
}

/***************************************************************************************************
 *                                     SECRET GAME
 ***************************************************************************************************/
static void SG_enter(void) {
  SG_resetRound();
}

static void SG_resetRound(void) {
  secretCode[0] = 0; secretCode[1] = 0; secretCode[2] = 0;
  guessCode[0] = 0;  guessCode[1] = 0;  guessCode[2] = 0;
  jamResultActive = false;
  jamDirections[0] = jamDirections[1] = jamDirections[2] = 0;
  selectedIdx = 0;
  attemptCount = 0;
  lastExact = 0;
  lastPartial = 0;
  secretLocked = false;
  bestExactThisRound = 0;
  secretGameState = SG_PICK_SECRET;
  prevExactForCombo = 0;
  hintUsedThisRound = false;
  jamUsedThisRound = false;
  jamArmed = false;
  SG_updatePcaLeds(0);
  SG_drawPickSecret();
}

static void SG_awardPointsAfterGuess(uint8_t exactNow) {
   uint8_t gained = 0;

  // points for the progress
  if (exactNow > bestExactThisRound) {
    gained = exactNow - bestExactThisRound;   
    totalScore += (long)gained * 5L;

    // combo if the previous guess has progress
    if (prevExactForCombo > 0) {
      totalScore += 10;
    }

    prevExactForCombo = 1; 
    bestExactThisRound = exactNow;
  } else {
    // no progress => no points, reset combo
    prevExactForCombo = 0;
  }
}

static void SG_drawPickSecret(void) {
  lcd.fillScreen(ST77XX_BLACK);
  LcdUtils_setCursor(0, 0);
  LcdUtils_printLine("SECRET CODE", CYAN, FONT_DEFAULT);
  LcdUtils_setCursor(0, 12);
  LcdUtils_printLine("Set 3 digits", WHITE, FONT_DEFAULT);
  LcdUtils_setCursor(0, 24);
  char b[24];
  sprintf(b, "%d %d %d", secretCode[0], secretCode[1], secretCode[2]);
  LcdUtils_printLine(b, YELLOW, FONT_FREE_MONO_9PT);

  // selector marker
  const int xCenters[3] = {6, 26, 48};
  int cx = xCenters[selectedIdx];
  lcd.fillRect(0, 46, 128, 10, ST77XX_BLACK);
  lcd.fillTriangle(cx - 5, 54, cx + 5, 54, cx, 47, GREEN);

  LcdUtils_setCursor(0, 58);
  LcdUtils_printLine("Joy L/R digit", WHITE, FONT_DEFAULT);
  LcdUtils_setCursor(0, 75);
  LcdUtils_printLine("Joy U/D select", WHITE, FONT_DEFAULT);
  LcdUtils_setCursor(0, 84);
  LcdUtils_printLine("SW3=lock", GREEN, FONT_DEFAULT);
}

static void SG_drawPickGuess(void) {
  lcd.fillScreen(ST77XX_BLACK);
  LcdUtils_setCursor(0, 0);
  LcdUtils_printLine("GUESS CODE", CYAN, FONT_DEFAULT);

  char sline[24];
  sprintf(sline, "Score:%ld", totalScore);
  LcdUtils_setCursor(0, 57);
  LcdUtils_printLine(sline, WHITE, FONT_DEFAULT);

  char t[20];
  sprintf(t, "Try %u/%u", (unsigned)(attemptCount + 1), (unsigned)maxAttempts);
  LcdUtils_setCursor(0, 12);
  LcdUtils_printLine(t, WHITE, FONT_DEFAULT);

  LcdUtils_setCursor(0, 24);
  char b[24];
  sprintf(b, "%d %d %d", guessCode[0], guessCode[1], guessCode[2]);
  LcdUtils_printLine(b, YELLOW, FONT_FREE_MONO_9PT);

  const int xCenters[3] = {6, 26, 48};
  int cx = xCenters[selectedIdx];
  lcd.fillRect(0, 46, 128, 10, ST77XX_BLACK);
  lcd.fillTriangle(cx - 5, 54, cx + 5, 54, cx, 47, GREEN);

  LcdUtils_setCursor(0, 60);
  LcdUtils_printLine("SW3=submit", GREEN, FONT_DEFAULT);
  LcdUtils_setCursor(0, 80);
  LcdUtils_printLine("SW2=reset", RED, FONT_DEFAULT);
}

static void SG_drawResult(void) {
  lcd.fillScreen(ST77XX_BLACK);
  LcdUtils_setCursor(0, 0);
  LcdUtils_printLine("RESULT", CYAN, FONT_DEFAULT);

  char b1[20], b2[20];
  sprintf(b1, "Exact: %u",   (unsigned)lastExact);
  sprintf(b2, "Partial: %u", (unsigned)lastPartial);

  LcdUtils_setCursor(0, 16);
  LcdUtils_printLine(b1, GREEN,  FONT_DEFAULT);
  LcdUtils_setCursor(0, 28);
  LcdUtils_printLine(b2, YELLOW, FONT_DEFAULT);

  char tries[20];
  sprintf(tries, "Used: %u/%u", (unsigned)attemptCount, (unsigned)maxAttempts);
  LcdUtils_setCursor(0, 40);
  LcdUtils_printLine(tries, WHITE, FONT_DEFAULT);

  char scoreLine[24];
  sprintf(scoreLine, "Score: %ld", totalScore);
  LcdUtils_setCursor(0, 52);
  LcdUtils_printLine(scoreLine, CYAN, FONT_DEFAULT);

  if (jamResultActive) {
    char radar[20];
    sprintf(radar, "Radar:%c %c %c",
      jamDirections[0] ==  0 ? '=' : (jamDirections[0] > 0 ? '^' : 'v'),
      jamDirections[1] ==  0 ? '=' : (jamDirections[1] > 0 ? '^' : 'v'),
      jamDirections[2] ==  0 ? '=' : (jamDirections[2] > 0 ? '^' : 'v'));
    LcdUtils_setCursor(0, 63);
    LcdUtils_printLine(radar, MAGENTA, FONT_DEFAULT);
  }

  if (lastExact == 3) {
    LcdUtils_setCursor(0, 75);
    LcdUtils_printLine("Code guessed!", GREEN, FONT_DEFAULT);
    LcdUtils_setCursor(0, 87);
    LcdUtils_printLine("SW3 -> SHOP",   WHITE, FONT_DEFAULT);

  } else if (attemptCount >= maxAttempts) {
    char s[24];
    sprintf(s, "Secret:%d%d%d", secretCode[0], secretCode[1], secretCode[2]);
    LcdUtils_setCursor(0, 75);
    LcdUtils_printLine("No tries left", RED,   FONT_DEFAULT);
    LcdUtils_setCursor(0, 87);
    LcdUtils_printLine(s,              WHITE,  FONT_DEFAULT);
    playLoseMelody();

  } else {
    LcdUtils_setCursor(0, 87);
    LcdUtils_printLine("SW3 continue", WHITE, FONT_DEFAULT);
  }
}


static void SG_drawWin(void) {
  lcd.fillScreen(ST77XX_BLACK);
  LcdUtils_setCursor(0, 26);
  LcdUtils_printLine("YOU WIN!", GREEN, FONT_FREE_MONO_9PT);
  LcdUtils_setCursor(0, 70);
  LcdUtils_printLine("SW3 -> SHOP", WHITE, FONT_DEFAULT);
  playWinMelody(); 
}

static void SG_drawShop(void) {
  lcd.fillScreen(ST77XX_BLACK);

  LcdUtils_setCursor(0, 0);
  LcdUtils_printLine("SHOP", CYAN, FONT_DEFAULT);

  char sbuf[24];
  sprintf(sbuf, "Score: %ld", totalScore);
  LcdUtils_setCursor(0, 12);
  LcdUtils_printLine(sbuf, YELLOW, FONT_DEFAULT);

  LcdUtils_setCursor(0, 30);
  LcdUtils_printLine("[SW1] Hint 30p", WHITE, FONT_DEFAULT);

  LcdUtils_setCursor(0, 44);
  LcdUtils_printLine("[SW4] Jam 50p", WHITE, FONT_DEFAULT);

  char inv[28];
  sprintf(inv, "Inv H:%u J:%u", hintInventory, jamInventory);
  LcdUtils_setCursor(0, 60);
  LcdUtils_printLine(inv, GREEN, FONT_DEFAULT);

  LcdUtils_setCursor(0, 84);
  LcdUtils_printLine("SW3 Continue", CYAN, FONT_DEFAULT);
}

static void SG_showHintBlink(uint8_t pos, uint8_t val) {
  uint8_t oldVal = guessCode[pos];   
  const int blinkCount = 4;

  for (int i = 0; i < 3; i++) {
    // ON
    guessCode[pos] = val;
    SG_drawPickGuess();
    delay(180);

    // OFF
    guessCode[pos] = oldVal;
    SG_drawPickGuess();
    delay(120);
  }

 guessCode[pos]=val;
 SG_drawPickGuess();
}

static void SG_handleJoystick(void) {
  if (!(secretGameState == SG_PICK_SECRET || secretGameState == SG_PICK_GUESS)) return;

  // LEFT/RIGHT axis
  int rawLR = analogRead(JOY_VRY_PIN);
  // UP/DOWN axis
  int rawUD = analogRead(JOY_VRX_PIN);

  if (millis() - lastJoyMoveMs < 160) return;

  const int TH = 650;

  // LEFT / RIGHT => move between digit positions
  if (rawLR > joyCenterX + TH) {
    selectedIdx = (selectedIdx == 0) ? 2 : (selectedIdx - 1);
    if (secretGameState == SG_PICK_SECRET) SG_drawPickSecret();
    else SG_drawPickGuess();
    lastJoyMoveMs = millis();
    return;
  }

  if (rawLR < joyCenterX - TH) {
    selectedIdx = (selectedIdx + 1) % 3;
    if (secretGameState == SG_PICK_SECRET) SG_drawPickSecret();
    else SG_drawPickGuess();
    lastJoyMoveMs = millis();
    return;
  }

  // UP / DOWN => change current digit value
  if (rawUD > joyCenterY + TH) {
    if (secretGameState == SG_PICK_SECRET) {
      secretCode[selectedIdx] = (secretCode[selectedIdx] == 0) ? 9 : (secretCode[selectedIdx] - 1);
      SG_drawPickSecret();
    } else {
      guessCode[selectedIdx] = (guessCode[selectedIdx] == 0) ? 9 : (guessCode[selectedIdx] - 1);
      SG_drawPickGuess();
    }
    lastJoyMoveMs = millis();
    return;
  }

  if (rawUD < joyCenterY - TH) {
    if (secretGameState == SG_PICK_SECRET) {
      secretCode[selectedIdx] = (secretCode[selectedIdx] + 1) % 10;
      SG_drawPickSecret();
    } else {
      guessCode[selectedIdx] = (guessCode[selectedIdx] + 1) % 10;
      SG_drawPickGuess();
    }
    lastJoyMoveMs = millis();
    return;
  }
}

static void SG_applyGuess(void) {
  if (!secretLocked) return;
  if (secretGameState != SG_PICK_GUESS) return;

  jamResultActive = false;   // clear previous radar before new guess

  attemptCount++;
  SG_evalGuess(guessCode, secretCode, &lastExact, &lastPartial);

  // JAM
  if (jamArmed) {
    jamResultActive = true;
    for (int i = 0; i < 3; i++) {
      if (guessCode[i] == secretCode[i]) {
        jamDirections[i] = 0;                          // exact: show '='
      } else if (secretCode[i] > guessCode[i]) {
        jamDirections[i] =  1;                         // secret is higher: show '^'
      } else {
        jamDirections[i] = -1;                         // secret is lower: show 'v'
      }
    }
    jamArmed = false;
  }

  SG_awardPointsAfterGuess(lastExact);
  SG_updatePcaLeds(lastExact);

  if (lastExact == 3) {
    totalScore += 25;
    secretGameState = SG_WIN;
    SG_drawWin();
  } else {
    secretGameState = SG_SHOW_RESULT;
    SG_drawResult();
  }
}

static void SG_evalGuess(uint8_t guess[3], uint8_t secret[3], uint8_t* exact, uint8_t* partial) {
  *exact = 0;
  *partial = 0;

  bool secretUsed[3] = {false, false, false};
  bool guessUsed[3]  = {false, false, false};

  for (int i = 0; i < 3; i++) {
    if (guess[i] == secret[i]) {
      (*exact)++;
      secretUsed[i] = true;
      guessUsed[i] = true;
    }
  }

  for (int i = 0; i < 3; i++) {
    if (guessUsed[i]) continue;
    for (int j = 0; j < 3; j++) {
      if (secretUsed[j]) continue;
      if (guess[i] == secret[j]) {
        (*partial)++;
        secretUsed[j] = true;
        guessUsed[i] = true;
        break;
      }
    }
  }
}

static void SG_updatePcaLeds(uint8_t value) {
  if (value > 8) value = 8;
  for (uint8_t i = 0; i < 8; i++) {
    if (i < value) PCA_enablePin(PCA_ADDRESS, i);
    else PCA_disablePin(PCA_ADDRESS, i);
  }
}

/***************************************************************************************************
 *                                     INIT FUNCTIONS
 ***************************************************************************************************/
static bool SERIAL_init(void) {
  Serial.begin(115200);
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 3000)) delay(10);
  delay(100);
  Serial.print("ifm Labz 2026\n");
  Serial.print("Board initialization...\n");
  return true;
}

static bool LCD_init(void) {
  pinMode(LCD_CS_PIN, OUTPUT);
  digitalWrite(LCD_CS_PIN, HIGH);

  pinMode(LCD_BCKL_PIN, OUTPUT);
  digitalWrite(LCD_BCKL_PIN, HIGH);

  pinMode(LCD_RST_PIN, OUTPUT);
  digitalWrite(LCD_RST_PIN, HIGH);
  delay(100);
  digitalWrite(LCD_RST_PIN, LOW);
  delay(100);
  digitalWrite(LCD_RST_PIN, HIGH);
  delay(200);

  SPI.begin();
  lcd.initR(INITR_TDO128x96);
  lcd.setSPISpeed(16000000UL);
  lcd.setRotation(0);
  lcd.fillScreen(ST77XX_BLACK);
  lcd.setTextWrap(false);
  lcd.setTextSize(1);

  return true;

}