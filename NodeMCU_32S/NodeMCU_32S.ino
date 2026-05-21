#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// WIFI SETTINGS
const char* ssid = "AndroidAP"; 
const char* password = "gata1234";
const char* serverIP = "10.118.144.124"; 
const int serverPort = 8080;

WiFiClient client;

// --- ESP32 HARDWARE PINS ---
// Joystick
#define JOY_VRX_PIN 36  
#define JOY_VRY_PIN 39  
#define JOY_BUTTON 32

// Game Buttons
#define SW1_PIN 33    
#define SW2_PIN 27     
#define SW3_PIN 14      
#define SW4_PIN 13      
#define BTN_PWR_PIN 4 
#define BUZZER_PIN 25

// OLED SPI PINS
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI  23 
#define OLED_CLK   18 
#define OLED_DC    16
#define OLED_RST   17
#define OLED_CS    5  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RST, OLED_CS);

// GAME VARIABLES 
uint8_t freqDigits[3] = {0, 0, 0};
uint8_t selectedIdx = 0;
String serverMessage = "WAITING SERVER...";
bool isConnectedToHost = false; // Devine true doar cand serverul ne conecteaza
bool isGameStarted = false;     // Devine true doar cand primim handshake de start

// --- Command rate limiting (prevents server spam) ---
static unsigned long lastBoostMs[4] = {0,0,0,0};
static unsigned long lastSubmitMs = 0;
bool lastJoyPressed = false;   // for joystick button edge detect 

const unsigned long BOOST_COOLDOWN_MS  = 250;  // one boost per 250ms
const unsigned long SUBMIT_COOLDOWN_MS = 400;  // one submit per 400ms

// Track joystick button edge (since it's polled, not interrupt)


unsigned long lastJoyMoveMs = 0;
int joyCenterX = 2048, joyCenterY = 2048;

// Timers
unsigned long lastDebugPrint = 0;
unsigned long lastOledRefresh = 0;
static unsigned long lastWifiReconnectAttempt = 0;
unsigned long lastReconnectAttempt = 0;

// Buzzer state variables (non-blocking beep)
unsigned long beepStartTime = 0;
int currentBeepDuration = 0;
bool isBeeping = false;

// ISR variables 
volatile bool B1Pressed = false, B2Pressed = false, B3Pressed = false, B4Pressed = false;
unsigned long lastISR_SW1 = 0, lastISR_SW2 = 0, lastISR_SW3 = 0, lastISR_SW4 = 0;
#define DEBOUNCE_MS 120

// PROTOTYPES
void drawTerminal();
void drawWaitingScreen();
void sendToServer(String msg);
void handleServerData();
void handleJoystick();
void debugHardware();

// Replaced LEDC with tone()
void playBeep(int freq = 1000, int duration = 100);
void playSong();
void handleBuzzer();

// ISR (Interrupts)
void IRAM_ATTR ISR_SW1() { if (millis() - lastISR_SW1 >= DEBOUNCE_MS) { lastISR_SW1 = millis(); B1Pressed = true; } }
void IRAM_ATTR ISR_SW2() { if (millis() - lastISR_SW2 >= DEBOUNCE_MS) { lastISR_SW2 = millis(); B2Pressed = true; } }
void IRAM_ATTR ISR_SW3() { if (millis() - lastISR_SW3 >= DEBOUNCE_MS) { lastISR_SW3 = millis(); B3Pressed = true; } }
void IRAM_ATTR ISR_SW4() { if (millis() - lastISR_SW4 >= DEBOUNCE_MS) { lastISR_SW4 = millis(); B4Pressed = true; } }

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- NEXUS SYSTEM BOOT ---");
  
  pinMode(SW1_PIN, INPUT_PULLUP); 
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP); 
  pinMode(SW4_PIN, INPUT_PULLUP);
  pinMode(BTN_PWR_PIN, INPUT_PULLUP); 
  pinMode(JOY_BUTTON, INPUT_PULLUP);

  // Buzzer pin: tone() will drive it, but set mode explicitly
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  attachInterrupt(digitalPinToInterrupt(SW1_PIN), ISR_SW1, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW2_PIN), ISR_SW2, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW3_PIN), ISR_SW3, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW4_PIN), ISR_SW4, FALLING);

  // START OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("OLED allocation failed! Verifica firele!"));
    for(;;); 
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print("WiFi Connecting...");
  display.display();

  // Calibrare Joystick
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 16; i++) { 
    sumX += analogRead(JOY_VRY_PIN); 
    sumY += analogRead(JOY_VRX_PIN); 
    delay(5); 
  }
  joyCenterX = sumX / 16; 
  joyCenterY = sumY / 16;
  Serial.printf("Joystick Calibrat: X=%d, Y=%d\n", joyCenterX, joyCenterY);

  // Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { 
      delay(500);
      Serial.print("."); 
      attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi OK.");
  }
}
void loop() {
  debugHardware();
  handleBuzzer();

  // --- WIFI ---
  if (WiFi.status() != WL_CONNECTED) {
    isConnectedToHost = false;
    isGameStarted = false;

    if (millis() - lastWifiReconnectAttempt > 5000) {
      Serial.println("[WIFI] Reconnecting...");
      WiFi.disconnect();
      WiFi.reconnect();
      lastWifiReconnectAttempt = millis();
    }

    if (millis() - lastOledRefresh > 200) {
      drawWaitingScreen();
      lastOledRefresh = millis();
    }
    return;
  }

  // --- TCP CONNECTED? ---
  if (client.connected()) {

 if (!isConnectedToHost) {
  isConnectedToHost = true;
  isGameStarted = true;              // start immediately
  sendToServer("CONNECT");
  serverMessage = "LINK ESTABLISHED";
  drawTerminal();
}
    // read server data
    if (client.available()) {
      handleServerData(); // sets isGameStarted when START/GAME_START arrives
    }

    // if game not started yet, just show waiting
    if (!isGameStarted) {
      if (millis() - lastOledRefresh > 200) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 10);
        display.print("CONNECTED TO HOST");
        display.setCursor(0, 30);
        
        display.display();
        lastOledRefresh = millis();
      }
      return;
    }

    // --- INPUTS (only after START) ---
    noInterrupts();
    bool b1 = B1Pressed; B1Pressed = false;
    bool b2 = B2Pressed; B2Pressed = false;
    bool b3 = B3Pressed; B3Pressed = false;
    bool b4 = B4Pressed; B4Pressed = false;
    interrupts();

    unsigned long now = millis();

    // SW1..SW4 -> BOOST:1..4
    if (b1 && (now - lastBoostMs[0] >= BOOST_COOLDOWN_MS)) { lastBoostMs[0] = now; sendToServer("BOOST:1"); playBeep(2000, 50); }
    if (b2 && (now - lastBoostMs[1] >= BOOST_COOLDOWN_MS)) { lastBoostMs[1] = now; sendToServer("BOOST:2"); playBeep(2000, 50); }
    if (b3 && (now - lastBoostMs[2] >= BOOST_COOLDOWN_MS)) { lastBoostMs[2] = now; sendToServer("BOOST:3"); playBeep(2000, 50); }
    if (b4 && (now - lastBoostMs[3] >= BOOST_COOLDOWN_MS)) { lastBoostMs[3] = now; sendToServer("BOOST:4"); playBeep(2000, 50); }

    // joystick move updates digits locally
    handleJoystick();

    // joystick button sends SUBMIT on press edge
    bool joyPressed = (digitalRead(JOY_BUTTON) == LOW);
    if (joyPressed && !lastJoyPressed) {
      if (now - lastSubmitMs >= SUBMIT_COOLDOWN_MS) {
        lastSubmitMs = now;
        int currentFreq = (freqDigits[0] * 100) + (freqDigits[1] * 10) + freqDigits[2];
        sendToServer("SUBMIT:" + String(currentFreq));
        serverMessage = "SUBMITTING...";
        playBeep(800, 150);
        drawTerminal();
      }
    }
    lastJoyPressed = joyPressed;

    return;
  }

  // --- TCP NOT CONNECTED: reconnect path ---
  if (isConnectedToHost || isGameStarted) {
    isConnectedToHost = false;
    isGameStarted = false;
    serverMessage = "DISCONNECTED";
    drawTerminal();
    playBeep(300, 200);
  }

  unsigned long now = millis();
  if (now - lastReconnectAttempt > 3000) {
    lastReconnectAttempt = now;
    client.stop();
    Serial.println("[TCP] Connecting...");
    client.connect(serverIP, serverPort);
  }

  if (millis() - lastOledRefresh > 200) {
    drawWaitingScreen();
    lastOledRefresh = millis();
  }
  
  
  

}
  /* --- S E R V E R ---
  if (client.connected()) {
  
    if (!isConnectedToHost) {
      isConnectedToHost = true;
      isGameStarted = true;  
      Serial.println("Socket conectat. Trimitem handshake...");
      sendToServer("CONNECT"); 
      serverMessage = "LINK ESTABLISHED"; 
      playSong();
      drawTerminal(); 
    }

    if (client.available()) {
      handleServerData();
    }

    if (isGameStarted) {
        noInterrupts();
        bool btn1 = B1Pressed; B1Pressed = false;
        bool btn2 = B2Pressed; B2Pressed = false;
        bool btn3 = B3Pressed; B3Pressed = false;
        bool btn4 = B4Pressed; B4Pressed = false;
        interrupts();

        static unsigned long lastPowerBtnPress = 0;
        if (digitalRead(BTN_PWR_PIN) == LOW && millis() - lastPowerBtnPress > 500) {
          Serial.println(">>> BUTONUL POWER A FOST APASAT! <<<");
          serverMessage = "SYS POWER TEST";
          playBeep(2500, 100);
          drawTerminal();
          lastPowerBtnPress = millis();
        }

        if(btn1) { sendToServer("BOOST:TRACER"); playBeep(2000, 50); }
        if(btn2) { sendToServer("BOOST:COOLDOWN"); playBeep(2000, 50); }
        if(btn4) { sendToServer("BOOST:BYPASS"); playBeep(2000, 50); }
        if(btn3) { 
          int currentFreq = (freqDigits[0]*100) + (freqDigits[1]*10) + freqDigits[2];
          sendToServer("SUBMIT:" + String(currentFreq));
          serverMessage = "SENDING...";
          playBeep(800, 150); 
          drawTerminal();
        }

        handleJoystick(); 
    } 
    else {
        if (millis() - lastOledRefresh > 200) {
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 10);
          display.print("CONNECTED TO HOST");
          display.setCursor(0, 30);
          display.print("Waiting for P2...");
          display.display();
          lastOledRefresh = millis();
        }
    }

  } else {
    if (isConnectedToHost || isGameStarted) { 
      isConnectedToHost = false;
      isGameStarted = false;
      Serial.println("Serverul a inchis conexiunea.");
      playBeep(300, 300); 
    }
    
    if (millis() - lastOledRefresh > 200) {
      drawWaitingScreen();
      lastOledRefresh = millis();
    }

    unsigned long now = millis();
    if (now - lastReconnectAttempt > 3000) {
        lastReconnectAttempt = now;
        client.connect(serverIP, serverPort);
    }
  }
}
*/

// --- tone() based buzzer ---

void playBeep(int freq, int duration) {
  // Start tone immediately; handleBuzzer() will stop it after duration
  tone(BUZZER_PIN, freq);
  beepStartTime = millis();
  currentBeepDuration = duration;
  isBeeping = true;
}

void playSong() {
  int melody[] = {
    659, // E5
    523, // C5
    587, // D5
    659, // E5
    587, // D5
    523, // C5
    440, // A4
    523, // C5
    659  // E5
  };

  int duration[] = {
    120,
    120,
    120,
    180,
    120,
    120,
    180,
    150,
    250
  };

  for (int i = 0; i < 9; i++) {
    tone(BUZZER_PIN, melody[i]);
    delay(duration[i]);

    noTone(BUZZER_PIN);
    delay(35);
  }

  noTone(BUZZER_PIN);
}

void handleBuzzer() {
  if (isBeeping && (millis() - beepStartTime >= (unsigned long)currentBeepDuration)) {
    noTone(BUZZER_PIN);
    isBeeping = false;
  }
}

// --- rest of your code unchanged ---

void debugHardware() {
  if (millis() - lastDebugPrint > 500) { 
    lastDebugPrint = millis();
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
    playBeep(4000, 20); 
    drawTerminal();
  }
}

void sendToServer(String msg) {
  if (client.connected()) {
    client.println("BETA|" + msg); 
    Serial.println("Sent: BETA|" + msg);
  }
}

void handleServerData() {
  String response = client.readStringUntil('\n');
  response.trim();
  if(response.length() > 0) {
    Serial.println("Receptat de la Server: " + response);

    if (!isGameStarted) {
      if (response == "START" || response == "GAME_START" || response == "LINK ESTABLISHED") {
        isGameStarted = true;
        serverMessage = "LINK ESTABLISHED";
        playBeep(1800, 300); 
        drawTerminal();
      }
      return; 
    }

    if(response.startsWith("DRIFT:")) {
      int driftVal = response.substring(6).toInt();
      int currentFreq = (freqDigits[0]*100) + (freqDigits[1]*10) + freqDigits[2];
      currentFreq += driftVal;
      if(currentFreq < 0) currentFreq = 0;
      if(currentFreq > 999) currentFreq = 999;
      
      freqDigits[0] = (currentFreq / 100) % 10;
      freqDigits[1] = (currentFreq / 10) % 10;
      freqDigits[2] = currentFreq % 10;
      serverMessage = "SYS DRIFT!";
      playBeep(3000, 500); 
    } 
    else if(response == "TOO LOW") {
      serverMessage = "ERROR: LOW FREQ";
      playBeep(300, 400);
    } 
    else if(response == "TOO HIGH") {
      serverMessage = "ERROR: HIGH FREQ";
      playBeep(300, 400);
    }
    else if(response == "MATCH") {
      serverMessage = "SECTOR UNLOCKED!";
      playBeep(2000, 300);
    }
    else {
      serverMessage = response; 
    }
    drawTerminal();
  }
}

void drawTerminal() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("NEXUS TERMINAL");
  
  display.setCursor(0, 15);
  display.print("FREQUENCY TX:");
  
  display.setTextSize(2);
  display.setCursor(0, 27);
  char b[24];
  sprintf(b, "%d %d %d Hz", freqDigits[0], freqDigits[1], freqDigits[2]);
  display.print(b);

  display.setTextSize(1);
  const int xCenters[3] = {6, 30, 54};
  display.setCursor(xCenters[selectedIdx], 45);
  display.print("^");

  display.drawRect(0, 54, 128, 10, SSD1306_WHITE);
  display.setCursor(2, 55);
  display.print(serverMessage.c_str());

  display.display(); 
}

void drawWaitingScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("OFFLINE - TEST MODE");
  
  display.setCursor(0, 20);
  display.printf("Joy X:%-4d Y:%-4d", analogRead(JOY_VRX_PIN), analogRead(JOY_VRY_PIN));

  display.setCursor(0, 35);
  display.printf("B1:%d B2:%d B3:%d B4:%d", 
    !digitalRead(SW1_PIN), !digitalRead(SW2_PIN), !digitalRead(SW3_PIN), !digitalRead(SW4_PIN));
  
  display.setCursor(0, 50);
  display.printf("PwrBtn:%d JoyBtn:%d", !digitalRead(BTN_PWR_PIN), !digitalRead(JOY_BUTTON));
  
  display.display();
}