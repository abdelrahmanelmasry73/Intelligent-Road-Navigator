#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <MFRC522.h>
#include <PCF8574.h>

// ---------------------------------------------------------------------------
// 1. PIN DEFINITIONS & CONSTANTS
// ---------------------------------------------------------------------------

// --- Ultrasonic Pins ---
const int US_TRIG_1 = 12; const int US_ECHO_1 = 34;
const int US_TRIG_2 = 13; const int US_ECHO_2 = 35;
const int US_TRIG_3 = 25; const int US_ECHO_3 = 36; // VP
const int US_TRIG_4 = 26; const int US_ECHO_4 = 39; // VN

// --- IR Pins ---
const int IR_1 = 27;
const int IR_2 = 16; // RX2
const int IR_3 = 17; // TX2
const int IR_4 = 32;

// --- RFID Pins (VSPI) ---
#define SS_PIN  5
#define RST_PIN 4
MFRC522 mfrc522(SS_PIN, RST_PIN);

// --- Arrow LED ---
const int ARROW_LEFT = 33;
const int ARROW_RIGHT = 14;

// --- I2C / PCF8574 ---
const int SDA_PIN = 21;
const int SCL_PIN = 22;
PCF8574 pcf1(0x21); // Expander 1: Traffic Lights
PCF8574 pcf2(0x20); // Expander 2: T3 Green + Buzzer

// --- Firebase Config ---
#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "YOUR_FIREBASE_DATABASE_URL" 

// --- Logic Constants ---
const int DIST_THRESHOLD = 20; // cm to trigger count
const int YELLOW_DURATION = 2000; // Fixed 2 seconds yellow

// ---------------------------------------------------------------------------
// 2. GLOBAL VARIABLES
// ---------------------------------------------------------------------------

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Counters
int laneCounts[4] = {0, 0, 0, 0};
bool laneTriggered[4] = {false, false, false, false};

// Traffic Light State Machine
enum TrafficState {
  T1_GREEN, T1_YELLOW,
  T2_GREEN, T2_YELLOW,
  T3_GREEN, T3_YELLOW
};
TrafficState currentState = T1_GREEN;
unsigned long stateStartTime = 0;

// Default Durations (can be overwritten by Firebase)
int dur_T1_Green = 5000;
int dur_T2_Green = 5000;
int dur_T3_Green = 5000;

// Buzzer & Arrow Control
bool buzzerActive = false;
String arrowState = "OFF"; // OFF, LEFT, RIGHT

// ---------------------------------------------------------------------------
// 3. HELPER FUNCTIONS
// ---------------------------------------------------------------------------

// Read Distance (Returns cm)
long readUltrasonic(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

// Update Traffic Lights via PCF
void updateTrafficLights() {
  // Logic: 0 = LED ON (Sink) or 1 = LED ON (Source)? 
  // Assuming Active HIGH (1=ON) for this example. If LEDs are inverted, swap HIGH/LOW.
  
  // Reset all to RED first for safety
  // PCF1 Mapping: P0=T1R, P1=T1Y, P2=T1G, P3=T2R, P4=T2Y, P5=T2G, P6=T3R, P7=T3Y
  // PCF2 Mapping: P0=T3G
  
  int pcf1_val = 0;
  int pcf2_val = 0; // Keeping buzzer state separate logic

  // Helper to set bits for PCF1
  auto setPCF1 = [&](int r1, int y1, int g1, int r2, int y2, int g2, int r3, int y3) {
    pcf1.write(0, r1); pcf1.write(1, y1); pcf1.write(2, g1);
    pcf1.write(3, r2); pcf1.write(4, y2); pcf1.write(5, g2);
    pcf1.write(6, r3); pcf1.write(7, y3);
  };

  // Helper for T3 Green on PCF2
  auto setT3Green = [&](int g3) {
    pcf2.write(0, g3); 
  };

  switch (currentState) {
    case T1_GREEN:
      setPCF1(0,0,1,  1,0,0,  1,0); // T1 G, T2 R, T3 R
      setT3Green(0);
      break;
    case T1_YELLOW:
      setPCF1(0,1,0,  1,0,0,  1,0); // T1 Y, T2 R, T3 R
      setT3Green(0);
      break;
    case T2_GREEN:
      setPCF1(1,0,0,  0,0,1,  1,0); // T1 R, T2 G, T3 R
      setT3Green(0);
      break;
    case T2_YELLOW:
      setPCF1(1,0,0,  0,1,0,  1,0); // T1 R, T2 Y, T3 R
      setT3Green(0);
      break;
    case T3_GREEN:
      setPCF1(1,0,0,  1,0,0,  0,0); // T1 R, T2 R, T3 G (on other chip)
      setT3Green(1);
      break;
    case T3_YELLOW:
      setPCF1(1,0,0,  1,0,0,  0,1); // T1 R, T2 R, T3 Y
      setT3Green(0);
      break;
  }
}

// ---------------------------------------------------------------------------
// 4. SETUP
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // --- Init Pins ---
  pinMode(US_TRIG_1, OUTPUT); pinMode(US_ECHO_1, INPUT);
  pinMode(US_TRIG_2, OUTPUT); pinMode(US_ECHO_2, INPUT);
  pinMode(US_TRIG_3, OUTPUT); pinMode(US_ECHO_3, INPUT);
  pinMode(US_TRIG_4, OUTPUT); pinMode(US_ECHO_4, INPUT);
  
  pinMode(IR_1, INPUT); pinMode(IR_2, INPUT);
  pinMode(IR_3, INPUT); pinMode(IR_4, INPUT);
  
  pinMode(ARROW_LEFT, OUTPUT); pinMode(ARROW_RIGHT, OUTPUT);

  // --- Init PCF8574 ---
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!pcf1.begin()) Serial.println("PCF1 Error");
  if (!pcf2.begin()) Serial.println("PCF2 Error");

  // --- Init RFID ---
  SPI.begin();
  mfrc522.PCD_Init();

  // --- WiFi & Firebase ---
  WiFi.begin("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected!");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  stateStartTime = millis();
}

// ---------------------------------------------------------------------------
// 5. MAIN LOOP
// ---------------------------------------------------------------------------

void loop() {
  unsigned long currentMillis = millis();

  // ==========================================
  // A. FETCH DATA FROM FIREBASE (Every 1 sec to avoid lag)
  // ==========================================
  static unsigned long lastFbRead = 0;
  if (currentMillis - lastFbRead > 1000) {
    lastFbRead = currentMillis;
    if (Firebase.ready()) {
      // 1. Get Traffic Durations
      Firebase.RTDB.getInt(&fbdo, "/config/t1_green_duration", &dur_T1_Green);
      Firebase.RTDB.getInt(&fbdo, "/config/t2_green_duration", &dur_T2_Green);
      Firebase.RTDB.getInt(&fbdo, "/config/t3_green_duration", &dur_T3_Green);
      
      // 2. Get Arrow State
      if (Firebase.RTDB.getString(&fbdo, "/control/arrow_led")) {
        arrowState = fbdo.stringData();
        if (arrowState == "LEFT") {
          digitalWrite(ARROW_LEFT, HIGH); digitalWrite(ARROW_RIGHT, LOW);
        } else if (arrowState == "RIGHT") {
          digitalWrite(ARROW_LEFT, LOW); digitalWrite(ARROW_RIGHT, HIGH);
        } else {
          digitalWrite(ARROW_LEFT, LOW); digitalWrite(ARROW_RIGHT, LOW);
        }
      }

      // 3. Get Buzzer State (P1 of Second Expander)
      bool buzVal = false;
      Firebase.RTDB.getBool(&fbdo, "/control/buzzer", &buzVal);
      pcf2.write(1, buzVal ? 1 : 0);
    }
  }

  // ==========================================
  // B. SENSOR COUNTING LOGIC (Interlock US + IR)
  // ==========================================
  
  // Define arrays for easy looping
  int trigPins[] = {US_TRIG_1, US_TRIG_2, US_TRIG_3, US_TRIG_4};
  int echoPins[] = {US_ECHO_1, US_ECHO_2, US_ECHO_3, US_ECHO_4};
  int irPins[]   = {IR_1, IR_2, IR_3, IR_4};

  for (int i = 0; i < 4; i++) {
    long dist = readUltrasonic(trigPins[i], echoPins[i]);
    // IR usually returns LOW when obstacle detected
    bool irDetected = (digitalRead(irPins[i]) == LOW); 

    // Logic: Both must be active
    if (dist < DIST_THRESHOLD && irDetected) {
      if (!laneTriggered[i]) {
        laneCounts[i]++;
        laneTriggered[i] = true;
        Serial.printf("Lane %d Triggered! Count: %d\n", i+1, laneCounts[i]);
        
        // Upload immediately
        String path = "/stats/lane_" + String(i+1) + "_count";
        Firebase.RTDB.setIntAsync(&fbdo, path.c_str(), laneCounts[i]);
      }
    } else {
      // Reset trigger when object leaves
      // Hysteresis: wait until dist is large AND IR is clear
      if (dist > (DIST_THRESHOLD + 5) || !irDetected) {
        laneTriggered[i] = false;
      }
    }
  }

  // ==========================================
  // C. RFID LOGIC
  // ==========================================
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uidStr = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      uidStr += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
      uidStr += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();
    Serial.println("RFID Tag: " + uidStr);
    
    // Upload RFID
    Firebase.RTDB.pushStringAsync(&fbdo, "/logs/rfid_entry", uidStr);
    
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  // ==========================================
  // D. TRAFFIC LIGHT STATE MACHINE
  // ==========================================
  unsigned long timeInState = currentMillis - stateStartTime;

  switch (currentState) {
    case T1_GREEN:
      if (timeInState > dur_T1_Green) {
        currentState = T1_YELLOW;
        stateStartTime = currentMillis;
      }
      break;
    case T1_YELLOW:
      if (timeInState > YELLOW_DURATION) {
        currentState = T2_GREEN;
        stateStartTime = currentMillis;
      }
      break;
    case T2_GREEN:
      if (timeInState > dur_T2_Green) {
        currentState = T2_YELLOW;
        stateStartTime = currentMillis;
      }
      break;
    case T2_YELLOW:
      if (timeInState > YELLOW_DURATION) {
        currentState = T3_GREEN;
        stateStartTime = currentMillis;
      }
      break;
    case T3_GREEN:
      if (timeInState > dur_T3_Green) {
        currentState = T3_YELLOW;
        stateStartTime = currentMillis;
      }
      break;
    case T3_YELLOW:
      if (timeInState > YELLOW_DURATION) {
        currentState = T1_GREEN; // Loop back
        stateStartTime = currentMillis;
      }
      break;
  }
  updateTrafficLights();
}
