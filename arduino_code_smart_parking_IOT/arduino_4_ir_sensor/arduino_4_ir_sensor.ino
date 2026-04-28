#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>

// Firebase helper (REQUIRED)
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// WIFI
#define WIFI_SSID "WtfKA41"
#define WIFI_PASSWORD "maotka122333122333"

// FIREBASE
#define API_KEY "AIzaSyATc2QeCvRqLgaeGDcEqC6NCw4Cfid42_U"
#define DATABASE_URL "https://smart-parking-capstone-ee166-default-rtdb.asia-southeast1.firebasedatabase.app/"

// IR PINS
#define IR1_PIN D1
#define IR2_PIN D2
#define IR3_PIN D5
#define IR4_PIN D6

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

int last1=-1, last2=-1, last3=-1, last4=-1;

// ---------------- WIFI ----------------
void connectWiFi() {
  Serial.print("[WAIT] WiFi connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retry++;

    if (retry > 25) {
      Serial.println("\n[ERROR] WiFi Failed → Restart");
      ESP.restart();
    }
  }

  Serial.println("\n[OK] WiFi Connected");
  Serial.println(WiFi.localIP());
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n===== SYSTEM START =====");

  pinMode(IR1_PIN, INPUT);
  pinMode(IR2_PIN, INPUT);
  pinMode(IR3_PIN, INPUT);
  pinMode(IR4_PIN, INPUT);
  Serial.println("[OK] Sensors Ready");

  connectWiFi();

  // FIREBASE CONFIG
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // OPTIONAL DEBUG
  config.token_status_callback = tokenStatusCallback;

  // SIGN IN (ANONYMOUS MODE)
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("[OK] Firebase SignUp Success");
    signupOK = true;
  } else {
    Serial.printf("[ERROR] SignUp Failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[OK] Firebase Initialized");
  Serial.println("========================\n");
}

// ---------------- LOOP ----------------
void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WARN] WiFi lost → reconnecting");
    connectWiFi();
  }

  int s1 = digitalRead(IR1_PIN);
  int s2 = digitalRead(IR2_PIN);
  int s3 = digitalRead(IR3_PIN);
  int s4 = digitalRead(IR4_PIN);

  Serial.printf("S1:%d | S2:%d | S3:%d | S4:%d\n", s1, s2, s3, s4);

  if (signupOK && Firebase.ready()) {

    if (s1!=last1 || s2!=last2 || s3!=last3 || s4!=last4) {

      Serial.println(">>> CHANGE DETECTED <<<");

      if (Firebase.RTDB.setInt(&fbdo, "/slots/slot1", s1) &&
          Firebase.RTDB.setInt(&fbdo, "/slots/slot2", s2) &&
          Firebase.RTDB.setInt(&fbdo, "/slots/slot3", s3) &&
          Firebase.RTDB.setInt(&fbdo, "/slots/slot4", s4)) {

        Serial.println("[OK] Firebase Updated");
      } else {
        Serial.print("[ERROR] Firebase: ");
        Serial.println(fbdo.errorReason());
      }

      last1=s1; last2=s2; last3=s3; last4=s4;
    }
  } else {
    Serial.println("[WAIT] Firebase not ready yet...");
  }

  delay(1000);
}