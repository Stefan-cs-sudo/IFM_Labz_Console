/***************************************************************************************************
 *                                     INCLUDES
 ***************************************************************************************************/
#include "SPI.h"
#include "Adafruit_ST7735.h"
#include "pca9557_cdd.h"
#include "StringUtils.h"
#include "LcdUtils.h"
#include "SD.h"

/***************************************************************************************************
 *                                     DEFINES — Timing
 ***************************************************************************************************/
#define CYCLE_TIME_1S 1000 /* 1s execution interval    */
#define CYCLE_TIME_10MS 10 /* 10ms execution interval  */
#define CYCLE_TIME_5MS 5   /* 5ms execution interval   */
#define CYCLE_TIME_MS 100  /* 100ms default wait time  */
#define CYCLE_TIME_1MS 1   /* 1ms execution interval   */

/***************************************************************************************************
 *                                     DEFINES — Push Buttons
 ***************************************************************************************************/
#define SW1_PIN 3 /* Digital pin for button SW1 */
#define SW2_PIN 2 /* Digital pin for button SW2 */
#define SW3_PIN 4 /* Digital pin for button SW3 */
#define SW4_PIN 9 /* Digital pin for button SW4 */

#define DEBOUNCE_MS 500 /* ISR debounce period in milliseconds */

/***************************************************************************************************
 *                                     DEFINES — Joystick
 ***************************************************************************************************/
#define JOY_VRX_PIN A0 /* Joystick VRx analog pin (controls up/down)    */
#define JOY_VRY_PIN A1 /* Joystick VRy analog pin (controls left/right) */
#define JOY_BUTTON A2  /* Joystick push button analog pin               */

/***************************************************************************************************
 *                                     DEFINES — LCD Pins
 *  Nano ESP32 requires Dx macros for correct GPIO mapping; fallback to raw numbers otherwise.
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
 *                                     DEFINES — SD Card
 ***************************************************************************************************/
#if defined(D8)
#define SD_CS_PIN D8
#else
#define SD_CS_PIN 8
#endif

#define SD_FILES_MAX_NUM 10      /* Maximum number of files readable from SD card */
#define SD_FILES_NAME_MAX_LEN 15 /* Maximum length of a file name on the SD card  */

/***************************************************************************************************
 *                                     DEFINES — PCA9557 I2C GPIO Expander
 ***************************************************************************************************/
#define PCA_ADDRESS 25 /* I2C address for GPIO expander (PCA9557) */

/***************************************************************************************************
 *                                     DEFINES — Menu System
 ***************************************************************************************************/
#define MENU_INIT 0U    /* Initialization screen                          */
#define MENU_DEFAULT 1U /* Default screen — shows time and ifmLabz 2026  */
#define MENU_IMAG 2U    /* SD card image viewer                          */
#define MENU_JOY_POS 3U /* Joystick position crosshair display           */
#define MENU_DICE 4U    /* Electronic dice game                          */
#define MENU_REAC 5U    /* Reaction time game                            */
#define MENU_MAX 6U     /* Total number of menu entries                  */

/***************************************************************************************************
 *                                     DEFINES — Reaction Game States
 ***************************************************************************************************/
#define REAC_IDLE 0U    /* Waiting for SW3 to start a round              */
#define REAC_WAITING 1U /* Red screen shown, random delay counting down   */
#define REAC_GO 2U      /* Screen flashed green, timing the user          */
#define REAC_RESULT 3U  /* Result displayed, waiting for SW3 to restart   */

/***************************************************************************************************
 *                                     DEFINES — Display Geometry
 ***************************************************************************************************/
#define SCREEN_W 128 /* LCD width in pixels  */
#define SCREEN_H 96  /* LCD height in pixels */
#define DOT_RADIUS 3 /* Joystick dot radius  */

/* Custom color not provided by Adafruit library */
#define ST77XX_GRAY 0x7BEF /* Medium gray in RGB565 */

/***************************************************************************************************
 *                                     GLOBAL VARIABLES — Menu & Timing
 ***************************************************************************************************/

unsigned int CurrentMenu = MENU_INIT; /* Index of the currently active menu screen */

unsigned long time0 = 0; /* Timestamp recorded at startup (for elapsed time) */
unsigned long time1 = 0; /* Timestamp recorded each second (for elapsed time) */

int randNumber = 0; /* Last generated dice roll value (1-6) */

/***************************************************************************************************
 *                                     GLOBAL VARIABLES — Reaction Game
 ***************************************************************************************************/

uint8_t reacState = REAC_IDLE;   /* Current reaction game state             */
unsigned long reacFlashTime = 0; /* millis() when screen turned green       */
unsigned long reacDelay = 0;     /* Random delay before green flash (ms)    */
unsigned long reacStartWait = 0; /* millis() when waiting period began      */
unsigned long reacP1Time = 0;    /* Player 1 (SW2) reaction time in ms      */
unsigned long reacP2Time = 0;    /* Player 2 (SW3) reaction time in ms      */
bool reacP1Done = false;         /* Player 1 has responded                  */
bool reacP2Done = false;         /* Player 2 has responded                  */

/***************************************************************************************************
 *                                     GLOBAL VARIABLES — Joystick
 ***************************************************************************************************/

int joyDotX = SCREEN_W / 2;     /* Current dot X position on screen        */
int joyDotY = SCREEN_H / 2;     /* Current dot Y position on screen        */
int joyPrevX = -1;              /* Previous dot X position (for erase)     */
int joyPrevY = -1;              /* Previous dot Y position (for erase)     */
bool joyCrosshairDrawn = false; /* Whether the crosshair has been drawn    */
int joyCenterX = 2048;          /* Calibrated VRy center ADC value (X)     */
int joyCenterY = 2048;          /* Calibrated VRx center ADC value (Y)     */

/***************************************************************************************************
 *                                     GLOBAL VARIABLES — SD Card & Images
 ***************************************************************************************************/

int PhotoIndex = 1;    /* Index of the current photo being displayed */
File SD_RootDir;       /* File handle for the SD card root directory */
int SDCardFilesNr = 0; /* Number of BMP files found on the SD card   */

char SD_files_list[SD_FILES_MAX_NUM][SD_FILES_NAME_MAX_LEN]; /* BMP file name list */
static unsigned int currentImageCount = 1;                   /* Currently displayed image index */

/***************************************************************************************************
 *                                     GLOBAL VARIABLES — Board State
 ***************************************************************************************************/

bool BoardInitialized = false; /* Whether the board has been properly initialized */
bool LcdBacklightOn = false;   /* LCD backlight activation status                 */
bool SchedulerStarted = false; /* Whether the cooperative scheduler has started   */

/***************************************************************************************************
 *                                     GLOBAL VARIABLES — Buttons (volatile, modified in ISRs)
 ***************************************************************************************************/

volatile bool B1Pressed = false; /* SW1 press flag */
volatile bool B2Pressed = false; /* SW2 press flag */
volatile bool B3Pressed = false; /* SW3 press flag */
volatile bool B4Pressed = false; /* SW4 press flag */

/* Debounce timestamps — last time each ISR was triggered */
volatile unsigned long lastISR_SW1 = 0;
volatile unsigned long lastISR_SW2 = 0;
volatile unsigned long lastISR_SW3 = 0;
volatile unsigned long lastISR_SW4 = 0;

/***************************************************************************************************
 *                                     GLOBAL VARIABLES — LCD Object
 ***************************************************************************************************/

/* RST = -1: manual reset required on Nano ESP32 (handled in LCD_init) */
Adafruit_ST7735 lcd = Adafruit_ST7735(LCD_CS_PIN, LCD_DC_PIN, -1);

/***************************************************************************************************
 *                                     FUNCTION PROTOTYPES — ISRs
 ***************************************************************************************************/

void IRAM_ATTR ISR_SW1(void); /* Interrupt service routine for SW1 */
void IRAM_ATTR ISR_SW2(void); /* Interrupt service routine for SW2 */
void IRAM_ATTR ISR_SW3(void); /* Interrupt service routine for SW3 */
void IRAM_ATTR ISR_SW4(void); /* Interrupt service routine for SW4 */

/***************************************************************************************************
 *                                     FUNCTION PROTOTYPES — Scheduler Tasks
 ***************************************************************************************************/

void Task1_10ms(void); /* 10ms periodic task — menu & button update    */
void Task2_5ms(void);  /* 5ms periodic task — joystick crosshair       */
void Task3_1s(void);   /* 1s periodic task — elapsed time display      */

/***************************************************************************************************
 *                                     FUNCTION PROTOTYPES — Initialization
 ***************************************************************************************************/

static bool LCD_init(void);    /* LCD display initialization              */
static bool SD_init(void);     /* SD card initialization                  */
static bool SERIAL_init(void); /* Serial communication initialization     */

/***************************************************************************************************
 *                                     FUNCTION PROTOTYPES — Buttons
 ***************************************************************************************************/

static void buttonReactSw1(void); /* SW1 press handler — menu previous     */
static void buttonReactSw2(void); /* SW2 press handler — image prev / etc  */
static void buttonReactSw3(void); /* SW3 press handler — image next / dice */
static void buttonReactSw4(void); /* SW4 press handler — menu next         */
static void processButtons(void); /* Read ISR flags and dispatch handlers   */

/***************************************************************************************************
 *                                     FUNCTION PROTOTYPES — Utilities
 ***************************************************************************************************/

static unsigned long computeDeltaTime(unsigned long StartTime, unsigned long EndTime);
static void TimeElapsed(void);

/***************************************************************************************************
 *                                     FUNCTION PROTOTYPES — SD Card Helpers
 ***************************************************************************************************/

static void SD_printDirectory(File Dir, int NumTabs);
static uint8_t SD_getFilesList(File Dir, char* FilesList, uint8_t maxListSize, uint8_t maxNameSize);

/***************************************************************************************************
 *                                     SETUP
 ***************************************************************************************************/

/*!
 * \brief One-time system initialization.
 *        Order: Serial -> Buttons -> LCD -> SD -> PCA -> RGB LED -> Joystick Cal -> SD files -> Interrupts
 */
void setup() {

  SERIAL_init();

  /* ---- Button pins ---- */
  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);
  pinMode(SW3_PIN, INPUT);
  pinMode(SW4_PIN, INPUT);


  /* ---- Built-in LED ---- */
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  /* ---- Deselect SD card CS before LCD init to prevent SPI bus conflicts ---- */
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  /* ---- LCD initialization ---- */
  Serial.println("LCD init...");
  if (LCD_init()) {
    Serial.println("LCD OK");
    LcdUtils_init(&lcd);
    BoardInitialized = true;
  } else {
    Serial.println("LCD FAILED");
    BoardInitialized = false;
    digitalWrite(LED_BUILTIN, HIGH);
    while (1)
      ; /* Halt on LCD failure */
  }

  /* ---- SD card initialization ---- */
  if (SD_init()) {
    Serial.println("SD OK");
  } else {
    Serial.println("SD FAILED");
  }
  delay(CYCLE_TIME_MS);

  /* ---- PCA9557 I2C GPIO Expander initialization ---- */
  if (PCA_initialize(PCA_ADDRESS)) {
    Serial.println("PCA OK");
  } else {
    Serial.println("PCA FAILED");
  }


  /* ---- RGB LED initialization (active-low, start as Magenta) ---- */
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  //ToDo: Study arduino nano esp32 schematic and light the onboard LED as MAGENTA
  digitalWrite(LEDR, LOW);
  digitalWrite(LEDG, HIGH);
  digitalWrite(LEDB, LOW);

  /* ---- Joystick auto-calibration (average 16 samples at rest) ---- */
  analogReadResolution(12);
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 16; i++) {
    sumX += analogRead(JOY_VRY_PIN);
    sumY += analogRead(JOY_VRX_PIN);
    delay(5);
  }
  joyCenterX = sumX / 16;
  joyCenterY = sumY / 16;
  Serial.print("Joy center X=");
  Serial.print(joyCenterX);
  Serial.print(" Y=");
  Serial.println(joyCenterY);

  /* ---- Random seed & time base ---- */
  randomSeed(analogRead(7));
  time0 = millis();

  /* ---- Set default menu ---- */
  CurrentMenu = MENU_DEFAULT;

  /* ---- Read SD card BMP file list ---- */
  Serial.println("Reading SD Card files");
  digitalWrite(LCD_CS_PIN, HIGH);
  SD_RootDir = SD.open("/");
  SDCardFilesNr = SD_getFilesList(SD_RootDir, &SD_files_list[0][0], SD_FILES_MAX_NUM, SD_FILES_NAME_MAX_LEN);
  for (int idx = 0; idx < SDCardFilesNr; idx++) {
    Serial.println((const char*)&SD_files_list[idx][0]);
  }
  digitalWrite(SD_CS_PIN, HIGH); /* Deselect SD after reading */

  /* ---- Full LCD hardware reset + re-init after SD SPI operations ---- */
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

  /* ---- Boot splash screen ---- */
  lcd.fillScreen(ST77XX_BLACK);
  LcdUtils_printLine("ifmLabz2026", RED, FONT_FREE_MONO_9PT);

  //ToDo: Attach button interrupts respecting the schematic
  attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, RISING);
  attachInterrupt(digitalPinToInterrupt(SW2_PIN), ISR_SW2, RISING);
  attachInterrupt(digitalPinToInterrupt(SW3_PIN), ISR_SW3, RISING);
  attachInterrupt(digitalPinToInterrupt(SW4_PIN), ISR_SW4, RISING);

  Serial.println("Setup complete");
}

/***************************************************************************************************
 *                                     MAIN LOOP — Cooperative Scheduler
 ***************************************************************************************************/

void loop() {
  static unsigned long previousMillis5ms = 0;
  static unsigned long previousMillis10ms = 0;
  static unsigned long previousMillis1s = 0;

  unsigned long currentMillis = millis();

  /* 10ms task — menu rendering, button processing, dice */
  if (computeDeltaTime(previousMillis10ms, currentMillis) >= CYCLE_TIME_10MS) {
    Task1_10ms();
    previousMillis10ms = currentMillis;
  }

  /* 5ms task — joystick crosshair update */
  if (computeDeltaTime(previousMillis5ms, currentMillis) >= CYCLE_TIME_5MS) {
    Task2_5ms();
    previousMillis5ms = currentMillis;
  }

  /* 1s task — elapsed time display */
  if (computeDeltaTime(previousMillis1s, currentMillis) >= CYCLE_TIME_1S) {
    Task3_1s();
    previousMillis1s = currentMillis;
  }

  SchedulerStarted = true;
}

/***************************************************************************************************
 *                                     SCHEDULER TASKS
 ***************************************************************************************************/

/*!
 * \brief 10ms periodic task.
 *        Handles button processing, menu transitions, joystick display, and dice game logic.
 */
void Task1_10ms(void) {
  processButtons();

  static unsigned int lMenu = MENU_INIT;
  static int oldRandNumber;

  /* ---- Menu transition: only runs when the menu changes ---- */
  if (lMenu != CurrentMenu) {

    //ToDo: Print on the serial monitor the current menu
    Serial.print("MENU:");
    Serial.println(CurrentMenu);

    switch (CurrentMenu) {
      case MENU_INIT:
        LcdUtils_clearScreen();
        Serial.println("init Screen displayed");
        break;

      case MENU_IMAG:
        /* Show first BMP image from SD card */
        currentImageCount = 0;
        LcdUtils_clearScreen();
        LcdUtils_bmpDraw(&SD_files_list[currentImageCount][0], 0, 0);
        LcdUtils_setCursor(0, 0);
        LcdUtils_printLine(&SD_files_list[currentImageCount][0], RED, FONT_FREE_MONO_9PT);
        break;

      case MENU_JOY_POS:
        /* Reset crosshair state so it redraws on entry */
        joyCrosshairDrawn = false;
        joyPrevX = -1;
        joyPrevY = -1;
        break;

      case MENU_DICE:
        LcdUtils_clearScreen();
        LcdUtils_setCursor(4, 0);
        LcdUtils_printLine("DiCe Game", GREEN, FONT_DEFAULT);
        break;

      case MENU_REAC:
        LcdUtils_clearScreen();
        LcdUtils_printLine("Reaction Game", GREEN, FONT_DEFAULT);
        LcdUtils_setCursor(0, 16);
        LcdUtils_printLine("P1:SW2  P2:SW3", RED, FONT_DEFAULT);
        LcdUtils_setCursor(0, 32);
        LcdUtils_printLine("SW3 to start", WHITE, FONT_DEFAULT);
        reacState = REAC_IDLE;
        reacP1Done = false;
        reacP2Done = false;
        break;

      default:
        LcdUtils_clearScreen();
        LcdUtils_setCursor(0, 0);
        LcdUtils_printLine("ifmLabz2026", RED, FONT_DEFAULT);
        CurrentMenu = MENU_DEFAULT;
        lMenu = MENU_DEFAULT;
        break;
    }
    lMenu = CurrentMenu;
  }



  /* ---- Dice game: update PCA LEDs and LCD when a new number is rolled ---- */
  if (CurrentMenu == MENU_DICE) {
    //ToDo: implement the dice game
    if (randNumber != 0 && randNumber != oldRandNumber) {
      /* Turn off all PCA LEDs first */
      for (int i = 1; i <= 6; i++) {
        PCA_disablePin(PCA_ADDRESS, i);
      }

      /* Light up LEDs 1 through randNumber */
      for (int i = 1; i <= randNumber; i++) {
        PCA_enablePin(PCA_ADDRESS, i);
        delay(1);
      }
      /* Clear the number area and print the new value */
      lcd.fillRect(0, 48, 40, 16, ST77XX_BLACK);
      LcdUtils_setCursor(0, 48);
      char charBuffer[3];
      itoa(randNumber, charBuffer, 10);
      LcdUtils_printLine(charBuffer, RED, FONT_FREE_MONO_9PT);
      oldRandNumber = randNumber;
    }
  }

  /* ---- Reaction game: state machine (runs every 10ms) ---- */
  if (CurrentMenu == MENU_REAC) {
    if (reacState == REAC_WAITING) {
      /* Check if the random delay has elapsed → flash green */
      if (millis() - reacStartWait >= reacDelay) {
        //ToDo : implement the funtionality discussed
        lcd.fillScreen(ST77XX_GREEN);
        reacFlashTime = millis();
        reacP1Done = false;
        reacP2Done = false;
        reacState = REAC_GO;
      }
    }
    /* Both players responded → show results */
    if (reacState == REAC_GO && reacP1Done && reacP2Done) {
      lcd.fillScreen(ST77XX_BLACK);
      char buf[10];
      /* Player 1 result */
      LcdUtils_setCursor(0, 0);
      LcdUtils_printLine("P1(SW2):", GREEN, FONT_DEFAULT);
      itoa(reacP1Time, buf, 10);
      LcdUtils_setCursor(0, 16);
      LcdUtils_printLine(buf, RED, FONT_FREE_MONO_9PT);
      LcdUtils_setCursor(48, 16);
      LcdUtils_printLine("ms", RED, FONT_DEFAULT);
      /* Player 2 result */

      //ToDo: implement the reslut statistic for the 2nd player
      LcdUtils_setCursor(0, 36);
      LcdUtils_printLine("P2(SW3):", GREEN, FONT_DEFAULT);
      itoa(reacP2Time, buf, 10);
      LcdUtils_setCursor(0, 52);
      LcdUtils_printLine(buf, RED, FONT_FREE_MONO_9PT);
      LcdUtils_setCursor(48, 52);
      LcdUtils_printLine("ms", RED, FONT_DEFAULT);

      /* Winner announcement */
      LcdUtils_setCursor(0, 72);
      if (reacP1Time < reacP2Time)
        LcdUtils_printLine(">> P1 wins!", WHITE, FONT_DEFAULT);
      else if (reacP2Time < reacP1Time)
        LcdUtils_printLine(">> P2 wins!", WHITE, FONT_DEFAULT);
      else
        LcdUtils_printLine(">> Tie!", WHITE, FONT_DEFAULT);
      LcdUtils_setCursor(0, 86);
      LcdUtils_printLine("SW3=again", GREEN, FONT_DEFAULT);
      reacState = REAC_IDLE;
    }
  }
}

/*!
 * \brief 5ms periodic task.  Reads joystick analog values and updates
 *        the crosshair dot position on the LCD while MENU_JOY_POS is active.
 */
void Task2_5ms(void) {
  int rawX, rawY;
  /* ---- Continuous joystick position update while on MENU_JOY_POS ---- */
  if (CurrentMenu == MENU_JOY_POS) {
    //ToDo: Read joystick analog values 
     rawX = analogRead(JOY_VRY_PIN); /* VRy controls left-right */
     rawY = analogRead(JOY_VRX_PIN); /* VRx controls up-down    */
    //optional print them on the Serial monitor

    /* Two-segment mapping with calibrated center: X axis inverted */
    if (rawX >= joyCenterX)
      joyDotX = map(rawX, joyCenterX, 4095, SCREEN_W / 2, DOT_RADIUS);
    else
      joyDotX = map(rawX, 0, joyCenterX, SCREEN_W - DOT_RADIUS - 1, SCREEN_W / 2);

    if (rawY <= joyCenterY)
      joyDotY = map(rawY, 0, joyCenterY, DOT_RADIUS, SCREEN_H / 2);
    else
      joyDotY = map(rawY, joyCenterY, 4095, SCREEN_H / 2, SCREEN_H - DOT_RADIUS - 1);

    //ToDo: Draw static crosshair once on first entry 
    if (!joyCrosshairDrawn) {
      lcd.fillScreen(ST77XX_BLACK);
      lcd.drawFastVLine(SCREEN_W / 2, 0, SCREEN_H, ST77XX_GRAY); /* Vertical center */
      lcd.drawFastHLine(0, SCREEN_H / 2, SCREEN_W, ST77XX_GRAY); /* Horizontal center */
      joyCrosshairDrawn = true;
    }
    // ToDo: Refresh when necessary
    /* Redraw dot only if it moved */
    if (joyDotX != joyPrevX || joyDotY != joyPrevY) {
      /* Erase old dot */
      if (joyPrevX >= 0) {
        lcd.fillCircle(joyPrevX, joyPrevY, DOT_RADIUS + 1, ST77XX_BLACK);
        /* Repair crosshair lines where the dot was erased */
        if (abs(joyPrevX - SCREEN_W / 2) <= DOT_RADIUS + 1)
          lcd.drawFastVLine(SCREEN_W / 2, joyPrevY - DOT_RADIUS - 1, (DOT_RADIUS + 1) * 2 + 1, ST77XX_GRAY);
        if (abs(joyPrevY - SCREEN_H / 2) <= DOT_RADIUS + 1)
          lcd.drawFastHLine(joyPrevX - DOT_RADIUS - 1, SCREEN_H / 2, (DOT_RADIUS + 1) * 2 + 1, ST77XX_GRAY);
      }
      /* Draw new dot */
      lcd.fillCircle(joyDotX, joyDotY, DOT_RADIUS, ST77XX_GREEN);
      joyPrevX = joyDotX;
      joyPrevY = joyDotY;
    }
  }
}

/*!
 * \brief 1s periodic task. Displays elapsed time since startup.
 */
void Task3_1s(void) {
  TimeElapsed();
}

/***************************************************************************************************
 *                                     INTERRUPT SERVICE ROUTINES
 *  IRAM_ATTR is required on ESP32 so ISR code resides in IRAM (prevents cache-miss crash).
 *  Each ISR uses a timestamp-based debounce to filter out contact bounce.
 ***************************************************************************************************/

void IRAM_ATTR ISR_SW1() {
  unsigned long now = millis();
  if (now - lastISR_SW1 >= DEBOUNCE_MS) {
    lastISR_SW1 = now;
  //ToDo: Update the flag accordingly
    B1Pressed = true;
  }
}

void IRAM_ATTR ISR_SW2() {
  unsigned long now = millis();
  if (now - lastISR_SW2 >= DEBOUNCE_MS) {
    lastISR_SW2 = now;
      //ToDo: Update the flag accordingly
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

/*!
 * \brief SW1 handler — Navigate to previous menu (wraps around).
 */
static void buttonReactSw1(void) {
  //ToDo: Menu decrementer
  if (CurrentMenu == MENU_DEFAULT) {
    CurrentMenu = MENU_MAX - 1;
  } else {
    CurrentMenu = CurrentMenu - 1;
  }
  Serial.println("SW1 pressed");
  Serial.println(CurrentMenu);
}

/*!
 * \brief SW2 handler — In MENU_IMAG: show previous image (wraps around).
 */
static void buttonReactSw2(void) {
  if (CurrentMenu == MENU_IMAG && SDCardFilesNr > 0) {
    if (currentImageCount == 0)
      currentImageCount = SDCardFilesNr - 1;
    else
      currentImageCount--;
    LcdUtils_clearScreen();
    LcdUtils_bmpDraw(&SD_files_list[currentImageCount][0], 0, 0);
    LcdUtils_setCursor(0, 0);
    LcdUtils_printLine(&SD_files_list[currentImageCount][0], RED, FONT_FREE_MONO_9PT);
  }
  if (CurrentMenu == MENU_REAC) {
    //ToDo: take the reaction time from the button press. Use ISR timestamp for accurate timing
    if (reacState == REAC_GO && !reacP1Done) {
      /* Player 1 reacted — use ISR timestamp for accurate timing */
      reacP1Time = lastISR_SW2 - reacFlashTime;
      reacP1Done = true;
    } else if (reacState == REAC_WAITING) {
      /* Player 1 pressed too early */
      lcd.fillScreen(ST77XX_BLACK);
      LcdUtils_setCursor(0, 32);
      LcdUtils_printLine("P1 too early!", RED, FONT_DEFAULT);
      LcdUtils_setCursor(0, 64);
      LcdUtils_printLine("SW3=retry", GREEN, FONT_DEFAULT);
      reacState = REAC_IDLE;
    }
  }
}

/*!
 * \brief SW3 handler — In MENU_IMAG: show next image. In MENU_DICE: roll the dice.
 */
static void buttonReactSw3(void) {
  if (CurrentMenu == MENU_IMAG && SDCardFilesNr > 0) {
    currentImageCount++;
    if (currentImageCount >= (unsigned int)SDCardFilesNr)
      currentImageCount = 0;
    LcdUtils_clearScreen();
    LcdUtils_bmpDraw(&SD_files_list[currentImageCount][0], 0, 0);
    LcdUtils_setCursor(0, 0);
    LcdUtils_printLine(&SD_files_list[currentImageCount][0], RED, FONT_FREE_MONO_9PT);
  }
  if (CurrentMenu == MENU_DICE) {
    //ToDo: implement random generator and print the number on the serial interface
    randNumber = random(1, 7);
    Serial.println(randNumber);
  }
  if (CurrentMenu == MENU_REAC) {
    //ToDo: implement the player 2 reaction time 
    if (reacState == REAC_IDLE) {
      /* Start a new round: random 2-5s delay before green flash */
      reacDelay = random(2000, 5001);
      reacStartWait = millis();
      reacState = REAC_WAITING;
      reacP1Done = false;
      reacP2Done = false;
      lcd.fillScreen(ST77XX_RED);
      LcdUtils_setCursor(0, 40);
      LcdUtils_printLine("Wait...", WHITE, FONT_DEFAULT);
    } else if (reacState == REAC_GO && !reacP2Done) {
      /* Player 2 reacted — use ISR timestamp for accurate timing */
      reacP2Time = lastISR_SW3 - reacFlashTime;
      reacP2Done = true;
    } else if (reacState == REAC_WAITING) {
      /* Player 2 pressed too early */
      lcd.fillScreen(ST77XX_BLACK);
      LcdUtils_setCursor(0, 32);
      LcdUtils_printLine("P2 too early!", RED, FONT_DEFAULT);
      LcdUtils_setCursor(0, 64);
      LcdUtils_printLine("SW3=retry", GREEN, FONT_DEFAULT);
      reacState = REAC_IDLE;
    }
  }
}

/*!
 * \brief SW4 handler — Navigate to next menu (wraps around).
 */
static void buttonReactSw4(void) {
  //ToDo: Menu incrementer
  if (CurrentMenu == MENU_MAX - 1) {
    CurrentMenu = MENU_DEFAULT;
  } else {
    CurrentMenu = CurrentMenu + 1;
  }
  Serial.println("SW4 pressed");
  Serial.println(CurrentMenu);
}

/*!
 * \brief Read button ISR flags (atomically) and dispatch the corresponding handlers.
 */
static void processButtons(void) {
  bool Button1, Button2, Button3, Button4;

  noInterrupts();
  Button1 = B1Pressed;
  B1Pressed = false;
  Button2 = B2Pressed;
  B2Pressed = false;
  Button3 = B3Pressed;
  B3Pressed = false;
  Button4 = B4Pressed;
  B4Pressed = false;
  interrupts();

//ToDo: call the respective functions

  if (Button1) buttonReactSw1();
  if (Button2) buttonReactSw2();
  if (Button3) buttonReactSw3();
  if (Button4) buttonReactSw4();
}

/***************************************************************************************************
 *                                     INITIALIZATION FUNCTIONS
 ***************************************************************************************************/

/*!
 * \brief Initialize Serial communication (USB-CDC on Nano ESP32).
 * \return true always.
 */
static bool SERIAL_init(void) {
  Serial.begin(115200);
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 3000)) {
    delay(10);
  }
  delay(100);
  Serial.print("ifm Labz 2026\n");
  Serial.print("Board initialization...\n");
  return true;
}

/*!
 * \brief Initialize the ST7735 LCD display.
 *        Performs manual hardware reset (RST constructor arg is -1 on ESP32).
 * \return true on success.
 */
static bool LCD_init(void) {
  pinMode(LCD_CS_PIN, OUTPUT);
  digitalWrite(LCD_CS_PIN, HIGH);

  pinMode(LCD_BCKL_PIN, OUTPUT);
  digitalWrite(LCD_BCKL_PIN, HIGH);

  /* Manual hardware reset pulse */
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

/*!
 * \brief Initialize the SD card on the shared SPI bus.
 *        Tries multiple SPI pin configs and frequencies (ESP32-specific).
 * \return true if SD card mounted successfully.
 */
static bool SD_init(void) {
  /* Deselect LCD before talking to SD on shared SPI bus */
  pinMode(LCD_CS_PIN, OUTPUT);
  digitalWrite(LCD_CS_PIN, HIGH);

  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(10);

#if defined(ARDUINO_ARCH_ESP32)
  const uint8_t csCandidates[] = { SD_CS_PIN };
  const uint32_t freqs[] = { 1000000UL, 400000UL };
  const int8_t spiSckPins[] = { -1, 13 };
  const int8_t spiMisoPins[] = { -1, 12 };
  const int8_t spiMosiPins[] = { -1, 11 };

  for (uint8_t spiCfgIdx = 0; spiCfgIdx < (sizeof(spiSckPins) / sizeof(spiSckPins[0])); spiCfgIdx++) {
    SD.end();
    SPI.end();
    delay(2);

    if (spiSckPins[spiCfgIdx] < 0) {
      SPI.begin();
      Serial.println("Trying SPI default pin mapping");
    } else {
      SPI.begin(spiSckPins[spiCfgIdx], spiMisoPins[spiCfgIdx], spiMosiPins[spiCfgIdx], SD_CS_PIN);
      Serial.print("Trying SPI pins SCK=");
      Serial.print(spiSckPins[spiCfgIdx]);
      Serial.print(" MISO=");
      Serial.print(spiMisoPins[spiCfgIdx]);
      Serial.print(" MOSI=");
      Serial.println(spiMosiPins[spiCfgIdx]);
    }

    for (uint8_t csIdx = 0; csIdx < (sizeof(csCandidates) / sizeof(csCandidates[0])); csIdx++) {
      uint8_t csPin = csCandidates[csIdx];

      for (uint8_t freqIdx = 0; freqIdx < (sizeof(freqs) / sizeof(freqs[0])); freqIdx++) {
        SD.end();
        delay(5);

        Serial.print("Trying SD.begin(cs=");
        Serial.print(csPin);
        Serial.print(", freq=");
        Serial.print(freqs[freqIdx]);
        Serial.println(")");

        if (!SD.begin(csPin, SPI, freqs[freqIdx])) continue;

        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE) {
          Serial.println("No SD card attached on this CS");
          SD.end();
          continue;
        }

        Serial.print("SD card type: ");
        if (cardType == CARD_MMC) Serial.println("MMC");
        else if (cardType == CARD_SD) Serial.println("SDSC");
        else if (cardType == CARD_SDHC) Serial.println("SDHC/SDXC");
        else Serial.println("UNKNOWN");

        /* Re-mount at higher frequency for faster reads */
        SD.end();
        delay(5);
        SD.begin(csPin, SPI, 4000000UL);
        return true;
      }
    }
  }

  Serial.println("All SD mount attempts failed");
  return false;
#else
  /* Non-ESP32 fallback */
  digitalWrite(LCD_CS_PIN, HIGH);
  return SD.begin(SD_CS_PIN);
#endif
}

/***************************************************************************************************
 *                                     UTILITY FUNCTIONS
 ***************************************************************************************************/

/*!
 * \brief Print elapsed time since startup to Serial and LCD (on MENU_DEFAULT).
 */
static void TimeElapsed() {
  //ToDo:  Calculate and print the time elapsed from the startup on serial monitor and also on the display
  time1 = millis();
  unsigned long deltaTime = computeDeltaTime(time0, time1);
  Serial.println(deltaTime);
  if (CurrentMenu == MENU_DEFAULT) {
    LcdUtils_clearScreen();
    LcdUtils_printLine("ifmLabz2026", RED, FONT_DEFAULT);
    LcdUtils_setCursor(0, 20);
    char charBuffer[20];
    itoa(deltaTime, charBuffer, 10);
    LcdUtils_printLine(charBuffer, RED, FONT_DEFAULT);
  }
}

/*!
 * \brief Compute delta time between two timestamps, handling unsigned overflow.
 * \param StartTime  Initial timestamp (ms).
 * \param EndTime    Final timestamp (ms).
 * \return Elapsed time in milliseconds.
 */
static unsigned long computeDeltaTime(unsigned long StartTime, unsigned long EndTime) {
  unsigned long Delta = 0;
  unsigned long Max = 0xFF;
  if (StartTime <= EndTime) {
    Delta = EndTime - StartTime;
  } else {
    Delta = StartTime - EndTime;
    for (int i = 0; i < (sizeof(unsigned long) - 1); i++) {
      Max = ((Max << 8) + 0xFF);
    }
    Delta = Max - Delta;
  }
  return Delta;
}

/***************************************************************************************************
 *                                     SD CARD HELPER FUNCTIONS
 ***************************************************************************************************/

/*!
 * \brief Recursively print SD card directory contents to Serial.
 * \param Dir      File object for the directory to list.
 * \param NumTabs  Indentation level (pass 0 on first call).
 */
static void SD_printDirectory(File Dir, int NumTabs) {
  while (true) {
    File entry = Dir.openNextFile();
    if (!entry) break;

    for (uint8_t i = 0; i < NumTabs; i++) Serial.print('\t');
    Serial.print(entry.name());

    if (entry.isDirectory()) {
      Serial.println("/");
      SD_printDirectory(entry, NumTabs + 1);
    } else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

/*!
 * \brief Read BMP file names from SD card root directory.
 *        Skips directories and non-.bmp files.
 * \param Dir          File object for the directory (from SD.open("/")).
 * \param FilesList    Pointer to pre-allocated array for file names.
 * \param maxListSize  Maximum number of files to store.
 * \param maxNameSize  Maximum characters per file name.
 * \return Number of BMP files found and stored.
 */
static uint8_t SD_getFilesList(File Dir, char* FilesList, uint8_t maxListSize, uint8_t maxNameSize) {
  uint8_t FNameIdx = 0;
  char* DestPtr = FilesList;
  File entry = Dir.openNextFile();
  const char* fname;

  while ((entry != false) && (FNameIdx < maxListSize)) {
    if (!entry.isDirectory()) {
      fname = entry.name();
      const char* dot = strrchr(fname, '.');
      if (dot && (strcasecmp(dot, ".bmp") == 0)) {
        StringUtils_stringCopy(fname, DestPtr, maxNameSize);
        FNameIdx++;
        DestPtr = (char*)((char*)DestPtr + maxNameSize);
      }
    }
    entry.close();
    entry = Dir.openNextFile();
  }
  return FNameIdx;
}
