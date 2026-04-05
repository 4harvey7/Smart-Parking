Project Name: Smart Parking System with RFID and Firebase


Hardware Requirements:

NodeMCU ESP8266 (Wi-Fi Microcontroller)
MFRC522 RFID Reader Module (13.56 MHz)
RFID Cards or Key Fobs
2x Infrared (IR) Obstacle Avoidance Sensors
Servo Motor (e.g., SG90 micro-servo or MG996R for a heavier gate)
Breadboard (for distributing power and ground connections)
Jumper Wires (Male-to-Female, Female-to-Female, Male-to-Male)
Micro-USB Data Cable (for programming the ESP8266)

Optional but recommended:
External 5V Power Supply or Battery Pack (Servos draw a lot of power and can sometimes restart the ESP8266 if they share the exact same power source)

Software & Cloud Requirements:

Arduino IDE (for writing and uploading the C++ code)
Google Firebase Account (Using Authentication and Realtime Database)
Visual Studio Code, Notepad++, or any text editor (for editing the HTML/JS dashboard files)
A Web Browser (Chrome, Edge, Safari) to view the dashboard

Required Arduino Libraries (Installed via Arduino Library Manager):

ESP8266 Board Package (Needs to be added to the Boards Manager URLs)
Firebase ESP8266 Client (Must be the exact one created by Mobizt)
MFRC522 (Created by GithubCommunity)
SPI (Built-in to Arduino)
Servo (Built-in to Arduino)
time (Built-in to ESP8266 core)

Web Dashboard Technologies Used:
HTML5
CSS3
Vanilla JavaScript
Firebase Web SDK (v8.10.1)
SweetAlert2 (For the floating popup notifications)





MFRC522 RFID Reader Pinout

SDA (SS) connects to D2
SCK connects to D5 (Hardware SPI)
MOSI connects to D7 (Hardware SPI)
MISO connects to D6 (Hardware SPI)
RST connects to D1
3.3V connects to 3V3 (Do not use 5V)
GND connects to GND
IRQ remains empty


IR Sensor 1 (Slot 1) Pinout

OUT (Signal) connects to D0
VCC (Power) connects to 3V3 or VIN
GND (Ground) connects to GND


IR Sensor 2 (Slot 2) Pinout

OUT (Signal) connects to D3
VCC (Power) connects to 3V3 or VIN
GND (Ground) connects to GND


Servo Motor (Gate) Pinout

Signal Wire (Yellow/Orange) connects to D4
Power Wire (Red) connects to VIN or VU (Must be 5V, do not use 3V3)
Ground Wire (Brown/Black) connects to GND

General Power Notes
All components must share the same Ground loop. You can use any available GND pin on the ESP8266.
If you run out of 3V3 or GND pins on the board, twist the wires together or use a breadboard to connect multiple wires to a single pin.
