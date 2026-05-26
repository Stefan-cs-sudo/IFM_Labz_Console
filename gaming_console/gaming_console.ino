#include <WiFi.h>
#include "SPI.h"
#include "Adafruit_ST7735.h"
#include "pca9557_cdd.h"
#include "LcdUtils.h"


const char* ssid = "AndroidAP"; 
const char* password = "gata1234";
const char* serverIP = "10.117.253.162"; 
const int serverPort = 8080;

WiFiClient client;

// HARDWARE PINS
#define SW1_PIN 3
#define SW2_PIN 2
#define SW3_PIN 4
#define SW4_PIN 9
#define JOY_VRX_PIN A0
#define JOY_VRY_PIN A1
#define JOY_BTN_PIN A2
#define LCD_CS_PIN 6
#define LCD_DC_PIN 7
#define LCD_RST_PIN 5
#define BUZZER_PIN 3
#define PCA_ADDRESS 25


bool lastBtnState = HIGH;
bool currentBtnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;


Adafruit_ST7735 lcd = Adafruit_ST7735(LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN);


uint8_t freqDigits[3] = {0, 0, 0};
uint8_t selectedIdx = 0;
String serverMessage = "CONNECTING...";
bool isConnectedToHost = false;
unsigned long lastJoyMoveMs = 0;
int joyCenterX = 2048, joyCenterY = 2048;


volatile bool B1Pressed = false, B2Pressed = false, B3Pressed = false, B4Pressed = false;
unsigned long lastISR_SW1 = 0, lastISR_SW2 = 0, lastISR_SW3 = 0, lastISR_SW4 = 0;
#define DEBOUNCE_MS 120

unsigned long lastReconnectAttempt = 0;


void drawTerminal();
void drawWaitingScreen();
void sendToServer(String msg);
void handleServerData();
void handleJoystick();
void handleConfirmation();
void handleDisconnection();
void handleConnectionSuccess();
void playBeep();


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
  pinMode(JOY_BTN_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW2_PIN), ISR_SW2, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW3_PIN), ISR_SW3, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW4_PIN), ISR_SW4, FALLING);

  SPI.begin();
  lcd.initR(INITR_BLACKTAB); 
  lcd.setRotation(0);
  lcd.fillScreen(ST77XX_BLACK);
  lcd.setTextSize(1);
  LcdUtils_init(&lcd);

 
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 16; i++) { sumX += analogRead(JOY_VRY_PIN); sumY += analogRead(JOY_VRX_PIN); delay(5); }
  joyCenterX = sumX / 16; joyCenterY = sumY / 16;

  
  LcdUtils_setCursor(0, 10);
  LcdUtils_printLine("WiFi Connecting...", YELLOW, FONT_DEFAULT);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  int attempts = 0;
while (WiFi.status() != WL_CONNECTED && attempts < 40) { 
    delay(500);
    Serial.print("Status: ");
    Serial.println(WiFi.status()); 
    attempts++;
}
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi OK. Looking for Server...");
    if (client.connect(serverIP, serverPort)) {
      isConnectedToHost = true;
      serverMessage = "NEXUS LINK OK";
      delay(200);
      sendToServer("CONNECT");

      detachInterrupt(digitalPinToInterrupt(SW1_PIN));

      tone(BUZZER_PIN, 1200, 100); 
      delay(200);
      tone(BUZZER_PIN, 2300, 150);
      delay(150);

      attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, FALLING);

      drawTerminal(); 
    } else {
      isConnectedToHost = false;

      detachInterrupt(digitalPinToInterrupt(SW1_PIN));

      tone(BUZZER_PIN, 600, 100); 
      delay(200);
      tone(BUZZER_PIN, 300, 150);
      delay(150);

      attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, FALLING);

      drawWaitingScreen(); 
    }
  } else {
    isConnectedToHost = false;
    drawWaitingScreen(); 
  }
}


void loop() {

  // check wifi
  if (WiFi.status() != WL_CONNECTED) {
    if (isConnectedToHost) {
      handleDisconnection(); 
    }
    Serial.println("WiFi Lost. Reconnecting...");
    WiFi.reconnect();
    delay(500);
    return;
  }

  
  if (!client.connected()) {
    if (isConnectedToHost) {
      handleDisconnection();
    }

  
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 3000) {
      lastReconnectAttempt = now;
      client.stop(); 
      yield();
     if (client.connect(serverIP, serverPort)) {
         Serial.println("Success!");
         delay(200);
         sendToServer("CONNECT");
      } else {
         Serial.println("Failed. Socket might be busy.");
      }
    }
    return; 
  }

  
  if (!isConnectedToHost) {
    handleConnectionSuccess();
  }

    noInterrupts();
    bool btn1 = B1Pressed; B1Pressed = false;
    bool btn2 = B2Pressed; B2Pressed = false;
    bool btn3 = B3Pressed; B3Pressed = false;
    bool btn4 = B4Pressed; B4Pressed = false;
    interrupts();

    if(btn1) sendToServer("BOOST:1"); // Overclock
    if(btn2) sendToServer("BOOST:2"); // Cold Reboot
    if(btn3) sendToServer("BOOST:3"); // Firewall Patch
    if(btn4) sendToServer("BOOST:4"); // Signal Filter

    handleJoystick(); 
    handleConfirmation();

    if (client.available()) {
      handleServerData();
    }
}

void handleJoystick() {
  static int filtLR = -1;
  static int filtUD = -1;

  int rawLR = analogRead(JOY_VRY_PIN);
  int rawUD = analogRead(JOY_VRX_PIN);

  
  if (filtLR < 0) filtLR = rawLR;
  if (filtUD < 0) filtUD = rawUD;

 
  filtLR = (filtLR * 7 + rawLR) / 8;
  filtUD = (filtUD * 7 + rawUD) / 8;

  if (millis() - lastJoyMoveMs < 160) return;

  
  const int DEADZONE = 900; 
  int dLR = filtLR - joyCenterX;
  int dUD = filtUD - joyCenterY;

  if (abs(dLR) < DEADZONE) dLR = 0;
  if (abs(dUD) < DEADZONE) dUD = 0;

  bool moved = false;

  if (dLR > 0) { selectedIdx = (selectedIdx == 0) ? 2 : (selectedIdx - 1); moved = true; }
  else if (dLR < 0) { selectedIdx = (selectedIdx + 1) % 3; moved = true; }

  if (dUD > 0) { freqDigits[selectedIdx] = (freqDigits[selectedIdx] == 0) ? 9 : (freqDigits[selectedIdx] - 1); moved = true; }
  else if (dUD < 0) { freqDigits[selectedIdx] = (freqDigits[selectedIdx] + 1) % 10; moved = true; }

  if (moved) {
    lastJoyMoveMs = millis();
    drawTerminal();
  }
}

void handleConfirmation() {
  int reading = digitalRead(JOY_BTN_PIN);

  if (reading != lastBtnState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && currentBtnState == HIGH) {

      int currentFreq = (freqDigits[0] * 100) + (freqDigits[1] * 10) + freqDigits[2];

      sendToServer("SUBMIT:" + String(currentFreq));

      serverMessage = "SUBMITTING...";
      drawTerminal();
      playBeep(); 
      
      Serial.print("Sent Confirmation: ");
      Serial.println(currentFreq);
    }
    currentBtnState = reading;
  }
  lastBtnState = reading;
}


void playWompWomp() {
  tone(BUZZER_PIN, 300, 200);
  delay(250);
  tone(BUZZER_PIN, 250, 200);
  delay(250);
  tone(BUZZER_PIN, 200, 400); 
}

void playBipBip() {
  tone(BUZZER_PIN, 2000, 100); 
  delay(150);
  tone(BUZZER_PIN, 2000, 100); 
}

void sendToServer(String msg) {
  if (client.connected()) {
    client.println("ALPHA|" + msg);
    Serial.println("Sent: ALPHA|  " + msg);
  }
}

void handleServerData() {
    String response = client.readStringUntil('\n');
    response.trim();
    
    if(response == "TOO LOW") {
        playWompWomp(); 
        serverMessage = "ERROR: LOW FREQ";
    } 
    else if(response == "TOO HIGH") {
        playBipBip();  
        serverMessage = "ERROR: HIGH FREQ";
    }
    else if(response == "MATCH") {
   
        tone(BUZZER_PIN, 1500, 100); delay(100);
        tone(BUZZER_PIN, 2000, 300);
        serverMessage = "SECTOR UNLOCKED!";
    }
    
    drawTerminal();
}

void handleDisconnection() {
  isConnectedToHost = false;
  client.stop(); 
  drawWaitingScreen();

  detachInterrupt(digitalPinToInterrupt(SW1_PIN));
  tone(BUZZER_PIN, 600, 100); delay(300);
  tone(BUZZER_PIN, 200, 300);
  attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, FALLING);
}

void handleConnectionSuccess() {
  isConnectedToHost = true;
  delay(200);
  sendToServer("CONNECT");
  serverMessage = "LINK ESTABLISHED";
  drawTerminal();

  detachInterrupt(digitalPinToInterrupt(SW1_PIN));
  tone(BUZZER_PIN, 1200, 100); delay(200);
  tone(BUZZER_PIN, 2300, 150); delay(150);
  attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, FALLING);
}

void drawTerminal() {
  lcd.fillScreen(ST77XX_BLACK);
  
  LcdUtils_setCursor(0, 5);
  LcdUtils_printLine("NEXUS TERMINAL", CYAN, FONT_DEFAULT);
  
  LcdUtils_setCursor(0, 20);
  LcdUtils_printLine("FREQUENCY TX:", WHITE, FONT_DEFAULT);
  

  LcdUtils_setCursor(0, 35);
  char b[24];
  sprintf(b, "%d %d %d Hz", freqDigits[0], freqDigits[1], freqDigits[2]);
  LcdUtils_printLine(b, YELLOW, FONT_FREE_MONO_9PT);


  const int xCenters[3] = {6, 26, 48};
  int cx = xCenters[selectedIdx];
  lcd.fillTriangle(cx - 5, 60, cx + 5, 60, cx, 53, GREEN);


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

