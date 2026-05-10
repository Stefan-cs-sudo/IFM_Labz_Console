#include <WiFi.h> // Pentru ESP32
#include "SPI.h"
#include "Adafruit_ST7735.h"
#include "pca9557_cdd.h"
#include "LcdUtils.h"

// WIFI SETTINGS
const char* ssid = "POCOX7Pro"; 
const char* password = "23062005";
const char* serverIP = "10.130.147.77"; // PHONE IP
const int serverPort = 8080;

WiFiClient client;

// HARDWARE PINS
#define SW1_PIN 3
#define SW2_PIN 2
#define SW3_PIN 4
#define SW4_PIN 9
#define JOY_VRX_PIN A0
#define JOY_VRY_PIN A1
#define JOY_BUTTON A2
#define LCD_CS_PIN 6
#define LCD_DC_PIN 7
#define LCD_RST_PIN 5
#define BUZZER_PIN 8 
#define PCA_ADDRESS 25

Adafruit_ST7735 lcd = Adafruit_ST7735(LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN);

// GAME VARIABLES 
uint8_t freqDigits[3] = {0, 0, 0};
uint8_t selectedIdx = 0;
String serverMessage = "CONNECTING...";
bool isConnectedToHost = false;
unsigned long lastJoyMoveMs = 0;
int joyCenterX = 2048, joyCenterY = 2048;

// ISR variables 
volatile bool B1Pressed = false, B2Pressed = false, B3Pressed = false, B4Pressed = false;
unsigned long lastISR_SW1 = 0, lastISR_SW2 = 0, lastISR_SW3 = 0, lastISR_SW4 = 0;
#define DEBOUNCE_MS 120

unsigned long lastReconnectAttempt = 0;

// PROTOTYPES
void drawTerminal();
void drawWaitingScreen();
void sendToServer(String msg);
void handleServerData();
void handleJoystick();
void playBeep();

// ISR
void IRAM_ATTR ISR_SW1() { if (millis() - lastISR_SW1 >= DEBOUNCE_MS) { lastISR_SW1 = millis(); B1Pressed = true; } }
void IRAM_ATTR ISR_SW2() { if (millis() - lastISR_SW2 >= DEBOUNCE_MS) { lastISR_SW2 = millis(); B2Pressed = true; } }
void IRAM_ATTR ISR_SW3() { if (millis() - lastISR_SW3 >= DEBOUNCE_MS) { lastISR_SW3 = millis(); B3Pressed = true; } }
void IRAM_ATTR ISR_SW4() { if (millis() - lastISR_SW4 >= DEBOUNCE_MS) { lastISR_SW4 = millis(); B4Pressed = true; } }

void setup() {
  Serial.begin(115200);

  int n = WiFi.scanNetworks();
Serial.println("Available networks:");
for (int i = 0; i < n; i++) {
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.println(" dBm)");
}
  
  pinMode(SW1_PIN, INPUT); pinMode(SW2_PIN, INPUT);
  pinMode(SW3_PIN, INPUT); pinMode(SW4_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);

  attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW2_PIN), ISR_SW2, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW3_PIN), ISR_SW3, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW4_PIN), ISR_SW4, FALLING);

  SPI.begin();
  lcd.initR(INITR_TDO128x96);
  lcd.setRotation(0);
  lcd.fillScreen(ST77XX_BLACK);
  lcd.setTextSize(1);
  LcdUtils_init(&lcd);

  // Init Joystick center
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 16; i++) { sumX += analogRead(JOY_VRY_PIN); sumY += analogRead(JOY_VRX_PIN); delay(5); }
  joyCenterX = sumX / 16; joyCenterY = sumY / 16;

  // Wi-Fi Connection
  LcdUtils_setCursor(0, 10);
  LcdUtils_printLine("WiFi Connecting...", YELLOW, FONT_DEFAULT);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  int attempts = 0;
while (WiFi.status() != WL_CONNECTED && attempts < 40) { 
    delay(500);
    Serial.print("Status: ");
    Serial.println(WiFi.status()); // 6 = WRONG_PASSWORD, 1 = NO_SSID
    attempts++;
}
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi OK. Looking for Server...");
    if (client.connect(serverIP, serverPort)) {
      isConnectedToHost = true;
      serverMessage = "NEXUS LINK OK";
      sendToServer("CONNECT");
      playBeep();
      drawTerminal(); 
    } else {
      isConnectedToHost = false;
      drawWaitingScreen(); 
    }
  } else {
    isConnectedToHost = false;
    drawWaitingScreen(); 
  }
}


void loop() {
  // try to reconnect
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(100);
    return;
  }

  // State logic - loby vs game
  if (client.connected()) {
    if (!isConnectedToHost) {
      isConnectedToHost = true;
      serverMessage = "LINK ESTABLISHED";
      drawTerminal(); 
      playBeep();
    }

    noInterrupts();
    bool btn1 = B1Pressed; B1Pressed = false;
    bool btn2 = B2Pressed; B2Pressed = false;
    bool btn3 = B3Pressed; B3Pressed = false;
    bool btn4 = B4Pressed; B4Pressed = false;
    interrupts();

    if(btn1) sendToServer("BOOST:TRACER");
    if(btn2) sendToServer("BOOST:COOLDOWN");
    if(btn4) sendToServer("BOOST:BYPASS");
    
    if(btn3) { 
      int currentFreq = (freqDigits[0]*100) + (freqDigits[1]*10) + freqDigits[2];
      sendToServer("SUBMIT:" + String(currentFreq));
      serverMessage = "SENDING...";
      drawTerminal();
      playBeep();
    }

    handleJoystick(); 

    if (client.available()) {
      handleServerData();
    }
    
  } else {
    if (isConnectedToHost || millis() < 2000) { 
      isConnectedToHost = false;
      drawWaitingScreen(); 
    }

    unsigned long now = millis();
    if (now - lastReconnectAttempt > 3000) {
        lastReconnectAttempt = now;
        client.connect(serverIP, serverPort);
    }
  }
}

void handleJoystick() {
  int rawLR = analogRead(JOY_VRY_PIN);
  int rawUD = analogRead(JOY_VRX_PIN);
  if (millis() - lastJoyMoveMs < 160) return;
  const int TH = 650;

  bool moved = false;
  if (rawLR > joyCenterX + TH) { selectedIdx = (selectedIdx == 0) ? 2 : (selectedIdx - 1); moved = true; }
  else if (rawLR < joyCenterX - TH) { selectedIdx = (selectedIdx + 1) % 3; moved = true; }
  
  if (rawUD > joyCenterY + TH) { freqDigits[selectedIdx] = (freqDigits[selectedIdx] == 0) ? 9 : (freqDigits[selectedIdx] - 1); moved = true; }
  else if (rawUD < joyCenterY - TH) { freqDigits[selectedIdx] = (freqDigits[selectedIdx] + 1) % 10; moved = true; }

  if(moved) {
    lastJoyMoveMs = millis();
    drawTerminal();
  }
}

void sendToServer(String msg) {
  if (client.connected()) {
    client.println("ALPHA|" + msg); // "BETA|" for the other console
    Serial.println("Sent: " + msg);
  }
}

void handleServerData() {
  String response = client.readStringUntil('\n');
  response.trim();
  if(response.length() > 0) {
    
    if(response.startsWith("DRIFT:")) {
      int driftVal = response.substring(6).toInt(); // ex: DRIFT:-10
      int currentFreq = (freqDigits[0]*100) + (freqDigits[1]*10) + freqDigits[2];
      currentFreq += driftVal;
      if(currentFreq < 0) currentFreq = 0;
      if(currentFreq > 999) currentFreq = 999;
      
      freqDigits[0] = (currentFreq / 100) % 10;
      freqDigits[1] = (currentFreq / 10) % 10;
      freqDigits[2] = currentFreq % 10;
      
      serverMessage = "SYS DRIFT ALERT!";
      playBeep();
    } else {
      serverMessage = response; // ex: "TOO LOW", "CORE UNLOCKED"
    }
    drawTerminal();
  }
}

void drawTerminal() {
  lcd.fillScreen(ST77XX_BLACK);
  
  LcdUtils_setCursor(0, 5);
  LcdUtils_printLine("NEXUS TERMINAL", CYAN, FONT_DEFAULT);
  
  LcdUtils_setCursor(0, 20);
  LcdUtils_printLine("FREQUENCY TX:", WHITE, FONT_DEFAULT);
  
  // DIGITS DRAW
  LcdUtils_setCursor(0, 35);
  char b[24];
  sprintf(b, "%d %d %d Hz", freqDigits[0], freqDigits[1], freqDigits[2]);
  LcdUtils_printLine(b, YELLOW, FONT_FREE_MONO_9PT);

  // MARKER JOYSTICK
  const int xCenters[3] = {6, 26, 48};
  int cx = xCenters[selectedIdx];
  lcd.fillTriangle(cx - 5, 60, cx + 5, 60, cx, 53, GREEN);

  // Server Message Box
  lcd.drawRect(0, 70, 128, 25, ST77XX_GRAY);
  LcdUtils_setCursor(3, 78);
  LcdUtils_printLine(serverMessage.c_str(), RED, FONT_DEFAULT);
}

void drawWaitingScreen() {
  lcd.fillScreen(ST77XX_BLACK);
  
  LcdUtils_setCursor(0, 30);
  LcdUtils_printLine("NEXUS OFFLINE", RED, FONT_DEFAULT);
  
  LcdUtils_setCursor(0, 50);
  LcdUtils_printLine("Waiting for", WHITE, FONT_DEFAULT);
  
  LcdUtils_setCursor(0, 65);
  LcdUtils_printLine("HOST SERVER...", YELLOW, FONT_DEFAULT);
}

void playBeep() {
  tone(BUZZER_PIN, 1000, 100);
  delay(100);
  noTone(BUZZER_PIN);
}