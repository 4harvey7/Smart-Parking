#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// Firebase helpers
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// WIFI
#define WIFI_SSID "WtfKA41"
#define WIFI_PASSWORD "maotka122333122333"

// FIREBASE
#define API_KEY "AIzaSyATc2QeCvRqLgaeGDcEqC6NCw4Cfid42_U"
#define DATABASE_URL "https://smart-parking-capstone-ee166-default-rtdb.asia-southeast1.firebasedatabase.app/"

// RFID
#define SS_PIN D2
#define RST_PIN D1

// SERVO
#define SERVO_PIN D0
#define OPEN_ANGLE 0
#define CLOSE_ANGLE 120

MFRC522 rfid(SS_PIN, RST_PIN);
Servo gateServo;

// Firebase
FirebaseData fbdo;
FirebaseData fbdo2; // Separate stream for slots to avoid conflicts
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

// ---------------- ERROR LOGGER ----------------
void logError(String msg) {
  Serial.println("\n[ERROR DETECTED] " + msg);
  if (Firebase.ready() && signupOK) {
    Firebase.RTDB.setString(&fbdo, "/latest_scan/error", msg);
  }
}

// ---------------- WIFI ----------------
void connectWiFi() {
  Serial.print("[WAIT] WiFi connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 20) {
      logError("WiFi connection timeout");
      return;
    }
  }
  Serial.println("\n[OK] WiFi Connected");
}

// ---------------- CHECK IF PARKING IS FULL ----------------
bool isParkingFull() {
  if (Firebase.RTDB.getJSON(&fbdo2, "/slots")) {
    String result = fbdo2.payload();
    result.trim();
    Serial.println("[DEBUG] Slots data: " + result);

    // Count how many slots are free (value = 1 means FREE, 0 means OCCUPIED)
    // If NO slot has value 1, parking is full
    // Simple check: if "1" does not appear in the slots JSON, all are occupied
    // But safer: count slot1..slot4 individually

    FirebaseJson json;
    json.setJsonData(result);

    FirebaseJsonData slot1, slot2, slot3, slot4;
    json.get(slot1, "slot1");
    json.get(slot2, "slot2");
    json.get(slot3, "slot3");
    json.get(slot4, "slot4");

    int s1 = slot1.success ? slot1.to<int>() : 0;
    int s2 = slot2.success ? slot2.to<int>() : 0;
    int s3 = slot3.success ? slot3.to<int>() : 0;// kuapl na sensor bali kupal
    int s4 = slot4.success ? slot4.to<int>() : 0;

    Serial.printf("[SLOTS] slot1=%d slot2=%d slot3=%d slot4=%d\n", s1, s2, s3, s4);

    // If ALL slots are 0 (occupied), parking is full
    if (s1 == 0 && s2 == 0 && s3 == 0 && s4 == 0) {
      Serial.println("[PARKING] ALL SLOTS FULL");
      return true;
    }

    return false;

  } else {
    logError("Failed to fetch slots: " + fbdo2.errorReason());
    // If we can't check, allow entry to be safe (or return true to block — your choice)
    return false;
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n===== RFID GATE + FIREBASE START =====");

  SPI.begin();
  rfid.PCD_Init();
  Serial.println("[OK] RFID Ready");

  gateServo.attach(SERVO_PIN);
  gateServo.write(CLOSE_ANGLE); // SAFE DEFAULT

  connectWiFi();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("[OK] Firebase SignUp Success");
    signupOK = true;
  } else {
    logError("Firebase SignUp Failed");
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[OK] Firebase Ready\n");
}

// ---------------- LOOP ----------------
void loop() {

  // WIFI CHECK
  if (WiFi.status() != WL_CONNECTED) {
    logError("WiFi Disconnected");
    connectWiFi();
    return;
  }

  // FIREBASE CHECK
  if (!signupOK || !Firebase.ready()) {
    delay(500);
    return;
  }

  // RFID CHECK
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) {
    logError("RFID read failed");
    return;
  }

  // ---------------- GET UID ----------------
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  uid.trim();

  Serial.println("\n[SCAN] UID: " + uid);
  rfid.PICC_HaltA();

  // ---------------- FIREBASE CARD SEARCH ----------------
  bool cardExists = false;
  bool openGate = false;
  String actionMsg = "Invalid Card";

  if (Firebase.RTDB.getJSON(&fbdo, "/users")) {
    String result = fbdo.payload();
    result.trim();
    Serial.println("[DEBUG] Searching for UID: " + uid);

    if (result.indexOf(uid) != -1) {
      cardExists = true;
      Serial.println("[DEBUG] UID found!");
    } else {
      Serial.println("[DEBUG] UID not found");
      logError("Card not found in database");
    }
  } else {
    logError("Firebase fetch failed: " + fbdo.errorReason());
  }

  // ---------------- PARKING FULL CHECK ----------------
  // Card is valid BUT we still check if parking is full before opening
  if (cardExists) {
    if (isParkingFull()) {
      actionMsg = "Parking Full";
      openGate = false; // KEEP GATE CLOSED even for valid card
      Serial.println("[GATE] STAY CLOSED (PARKING FULL)");
    } else {
      actionMsg = "Access Granted";
      openGate = true;
    }
  }

  Serial.println("[ACTION] " + actionMsg);

  // ---------------- SAVE LOG ----------------
  Firebase.RTDB.setString(&fbdo, "/latest_scan/uid", uid);
  Firebase.RTDB.setString(&fbdo, "/latest_scan/action", actionMsg);

  // ---------------- SERVO CONTROL ----------------
  if (openGate) {
    Serial.println("[GATE] OPEN");
    gateServo.write(OPEN_ANGLE);
    delay(5000);
    gateServo.write(CLOSE_ANGLE);
    Serial.println("[GATE] CLOSED");
  } else {
    Serial.println("[GATE] STAY CLOSED");
  }

  delay(500);
}