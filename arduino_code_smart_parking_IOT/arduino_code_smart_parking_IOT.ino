#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <time.h> 

// ===== WIFI & FIREBASE CONFIG =====
#define WIFI_SSID "LicayanWifi(Router #2)"
#define WIFI_PASSWORD "licayan#1"

#define API_KEY "AIzaSyATc2QeCvRqLgaeGDcEqC6NCw4Cfid42_U"
#define DATABASE_URL "smart-parking-capstone-ee166-default-rtdb.asia-southeast1.firebasedatabase.app"

// ===== PIN CONFIG (SAFE VERSION) =====
// RFID (SPI fixed pins)
#define SS_PIN D2
#define RST_PIN D1

// IR SENSORS (SAFE BOOT PINS)
#define IR1_PIN D3
#define IR2_PIN D4
#define IR3_PIN D8

#define SERVO_PIN D0 

MFRC522 rfid(SS_PIN, RST_PIN);
Servo gateServo;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ===== STATES =====
int lastIr1State = -1;
int lastIr2State = -1;
int lastIr3State = -1;

String registeredUserUID = "32B85055";

void setup() {
  Serial.begin(115200);
  Serial.println("\n🔥 BOOT OK - SYSTEM START");

  SPI.begin();
  rfid.PCD_Init();
  Serial.println("[OK] RFID Ready");

  pinMode(IR1_PIN, INPUT);
  pinMode(IR2_PIN, INPUT);
  pinMode(IR3_PIN, INPUT);

  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  Serial.println("[OK] Sensors Ready");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WAIT] WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[OK] WiFi Connected");

  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("[WAIT] Time");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[OK] Time OK");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = "";
  auth.user.password = "";
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[OK] Firebase Ready");
}

void loop() {

  int currentIr1 = digitalRead(IR1_PIN);
  int currentIr2 = digitalRead(IR2_PIN);
  int currentIr3 = digitalRead(IR3_PIN);

  if (currentIr1 != lastIr1State ||
      currentIr2 != lastIr2State ||
      currentIr3 != lastIr3State) {

    Serial.println("\n>>> SENSOR UPDATE <<<");

    Firebase.setInt(fbdo, "/slots/slot1", currentIr1);
    Firebase.setInt(fbdo, "/slots/slot2", currentIr2);
    Firebase.setInt(fbdo, "/slots/slot3", currentIr3);

    lastIr1State = currentIr1;
    lastIr2State = currentIr2;
    lastIr3State = currentIr3;
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    rfid.PICC_HaltA();

    String actionMsg = "";
    bool openGate = false;

    if (uid == registeredUserUID) {

      if (currentIr1 == 0 && currentIr2 == 0 && currentIr3 == 0) {
        actionMsg = "Lot Full";
      } else {
        actionMsg = "Access Granted";
        openGate = true;
      }

    } else {
      actionMsg = "Invalid Card";
    }

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    char buffer[30];
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            t->tm_year + 1900,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec);

    String scanTime = String(buffer);

    Firebase.setString(fbdo, "/latest_scan/uid", uid);
    Firebase.setString(fbdo, "/latest_scan/action", actionMsg);
    Firebase.setString(fbdo, "/latest_scan/time", scanTime);

    FirebaseJson log;
    log.add("uid", uid);
    log.add("action", actionMsg);
    log.add("time", scanTime);
    Firebase.pushJSON(fbdo, "/logs", log);

    if (openGate) {
      gateServo.write(90);
      delay(5000);
      gateServo.write(0);
    }
  }
}