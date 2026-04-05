#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <time.h> 

// ===== WIFI & FIREBASE CONFIG =====
#define WIFI_SSID "Licayan Family"
#define WIFI_PASSWORD "tancing#919"

#define API_KEY "AIzaSyATc2QeCvRqLgaeGDcEqC6NCw4Cfid42_U"
#define DATABASE_URL "smart-parking-capstone-ee166-default-rtdb.asia-southeast1.firebasedatabase.app"

// ===== PINS CONFIGURATION =====
#define SS_PIN D2
#define RST_PIN D1
#define IR1_PIN D0
#define IR2_PIN D3   
#define SERVO_PIN D4 

MFRC522 rfid(SS_PIN, RST_PIN);
Servo gateServo;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int lastIr1State = -1;
int lastIr2State = -1;

String registeredUserUID = "32B85055"; 

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n--- SYSTEM STARTING ---");

  SPI.begin();
  rfid.PCD_Init();
  Serial.println("[OK] RFID Reader Initialized.");

  pinMode(IR1_PIN, INPUT);
  pinMode(IR2_PIN, INPUT);
  gateServo.attach(SERVO_PIN);
  gateServo.write(0); 
  Serial.println("[OK] Sensors & Servo Initialized.");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WAIT] Connecting to WiFi: ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[OK] WiFi Connected! IP: " + WiFi.localIP().toString());

  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("[WAIT] Syncing Internet Time...");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[OK] Time Synced!");

  Serial.println("[WAIT] Connecting to Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.signUp(&config, &auth, "", ""); 
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("[OK] Firebase Setup Complete.");
  Serial.println("--- SETUP FINISHED. READY FOR SCANS! ---\n");
}

void loop() {
  // 1. Read IR Sensors
  int currentIr1 = digitalRead(IR1_PIN);
  int currentIr2 = digitalRead(IR2_PIN);

  // 2. Instantly update Firebase if a car parks or leaves
  if (currentIr1 != lastIr1State || currentIr2 != lastIr2State) {
    Serial.println("\n>>> SENSOR CHANGE DETECTED <<<");
    Serial.print("Slot 1 is now: "); Serial.println(currentIr1);
    Serial.print("Slot 2 is now: "); Serial.println(currentIr2);

    if (Firebase.setInt(fbdo, "/slots/slot1", currentIr1)) {
      Serial.println("[SUCCESS] Slot 1 updated in Firebase.");
    } else {
      Serial.println("[ERROR] Failed to update Slot 1: " + fbdo.errorReason());
    }

    if (Firebase.setInt(fbdo, "/slots/slot2", currentIr2)) {
      Serial.println("[SUCCESS] Slot 2 updated in Firebase.");
    } else {
      Serial.println("[ERROR] Failed to update Slot 2: " + fbdo.errorReason());
    }

    lastIr1State = currentIr1;
    lastIr2State = currentIr2;
    delay(500); 
  }

  // 3. Handle RFID Scans
  if (rfid.PICC_IsNewCardPresent()) {
    if (rfid.PICC_ReadCardSerial()) {
      Serial.println("\n>>> RFID CARD SCANNED <<<");
      
      String uid = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        uid += String(rfid.uid.uidByte[i], HEX);
      }
      uid.toUpperCase();
      Serial.println("UID: " + uid);
      rfid.PICC_HaltA(); 

      String actionMsg = "";
      bool openGate = false; // 🚨 NEW: Flag to control the gate AFTER Firebase updates

      // --- DETERMINE LOGIC (But don't open the gate yet!) ---
      if (uid == registeredUserUID) {
        if (currentIr1 == 0 && currentIr2 == 0) { 
          actionMsg = "Access Denied - Lot Full";
          Serial.println("Gate Logic: ❌ Access Denied: Parking Lot is FULL.");
        } else {
          actionMsg = "Access Granted - Welcome!";
          Serial.println("Gate Logic: ✅ Access Granted!");
          openGate = true; // Set the flag to open the gate later
        }
      } else {
        actionMsg = "Access Denied - Unregistered Card";
        Serial.println("Gate Logic: ❌ Access Denied: Unregistered Card.");
      }

      // --- GET CURRENT TIME ---
      time_t now = time(nullptr);
      struct tm* p_tm = localtime(&now);
      char timeBuffer[30];
      sprintf(timeBuffer, "%04d-%02d-%02d %02d:%02d:%02d", 
              p_tm->tm_year + 1900, p_tm->tm_mon + 1, p_tm->tm_mday, 
              p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
      String scanTime = String(timeBuffer);
      Serial.println("Recorded Time: " + scanTime);

      // --- 🚀 INSTANTLY SEND TO FIREBASE FIRST ---
      Serial.println("[WAIT] Sending scan details to Firebase...");

      bool success1 = Firebase.setString(fbdo, "/latest_scan/uid", uid);
      bool success2 = Firebase.setString(fbdo, "/latest_scan/action", actionMsg);
      bool success3 = Firebase.setString(fbdo, "/latest_scan/time", scanTime); // 🚨 NEW: Forces dashboard to detect a change
      
      if(success1 && success2 && success3) {
        Serial.println("[SUCCESS] /latest_scan updated.");
      } else {
        Serial.println("[ERROR] /latest_scan update failed: " + fbdo.errorReason());
      }
      
      FirebaseJson jsonLog;
      jsonLog.add("uid", uid);
      jsonLog.add("action", actionMsg);
      jsonLog.add("time", scanTime);
      
      if(Firebase.pushJSON(fbdo, "/logs", jsonLog)) {
        Serial.println("[SUCCESS] New log added to /logs history.");
      } else {
        Serial.println("[ERROR] Failed to push to /logs: " + fbdo.errorReason());
      }
      
      Serial.println(">>> FIREBASE UPDATED INSTANTLY <<<");

      // --- 🚪 OPEN THE GATE LAST ---
      if (openGate) {
          Serial.println("Opening Gate now...");
          gateServo.write(90);  
          delay(5000); // Wait 5 seconds while the gate is open      
          gateServo.write(0);   
          Serial.println("Gate Closed.");
      }

      Serial.println(">>> SCAN PROCESSING COMPLETE <<<");
      delay(1000); 
    }
  }
}