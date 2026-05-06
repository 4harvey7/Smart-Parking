#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

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
FirebaseData fbdo2; 
FirebaseAuth auth;
FirebaseConfig config;

// NTP Time Setup
WiFiUDP ntpUDP;
// Offset for Philippines (UTC+8) = 8 * 3600 = 28800
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800);

bool signupOK = false;

// ---------------- WIFI ----------------
void connectWiFi() {
  Serial.print("[WAIT] WiFi connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[OK] WiFi Connected");
  timeClient.begin(); // Start NTP
}

// ---------------- GET FORMATTED TIME ----------------
String getFormattedDateTime() {
  timeClient.update();
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime(&rawtime);

  char buffer[25];
  // Formats to: YYYY-MM-DD HH:MM:SS
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", 
          ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday, 
          ti->tm_hour, ti->tm_min, ti->tm_sec);
  return String(buffer);
}

// ---------------- CHECK IF PARKING IS FULL ----------------
bool isParkingFull() {
  if (Firebase.RTDB.getJSON(&fbdo2, "/slots")) {
    FirebaseJson json;
    json.setJsonData(fbdo2.payload());
    FirebaseJsonData s1, s2, s3, s4;
    json.get(s1, "slot1"); json.get(s2, "slot2");
    json.get(s3, "slot3"); json.get(s4, "slot4");
    if (s1.to<int>() == 0 && s2.to<int>() == 0 && s3.to<int>() == 0 && s4.to<int>() == 0) return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  gateServo.attach(SERVO_PIN);
  gateServo.write(CLOSE_ANGLE); 
  connectWiFi();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) signupOK = true;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) return;

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  rfid.PICC_HaltA();

  bool cardExists = false;
  bool openGate = false;
  String actionMsg = "Invalid Card";

  if (Firebase.RTDB.getJSON(&fbdo, "/users")) {
    if (fbdo.payload().indexOf(uid) != -1) cardExists = true;
  }

  if (cardExists) {
    if (isParkingFull()) actionMsg = "Parking Full";
    else { actionMsg = "Access Granted"; openGate = true; }
  }

  // ---------------- SAVE LOG AS STRING ----------------
  String currentDateTime = getFormattedDateTime();
  FirebaseJson logData;
  logData.add("uid", uid);
  logData.add("action", actionMsg);
  logData.add("time", currentDateTime); // Stores as "YYYY-MM-DD HH:MM:SS"

  Firebase.RTDB.setJSON(&fbdo, "/latest_scan", &logData);
  Firebase.RTDB.pushJSON(&fbdo, "/logs", &logData);

  if (openGate) {
    gateServo.write(OPEN_ANGLE);
    delay(5000);
    gateServo.write(CLOSE_ANGLE);
  }
  delay(500);
}