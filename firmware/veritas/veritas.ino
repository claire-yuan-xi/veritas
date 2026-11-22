#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

// firmware for the Seeed XIAO ESP32-S3

// LED pins (one LED per color of light)
const int LED_RED = D0;
const int LED_GREEN = D1;
const int LED_BLUE = D2;
const int LED_IR = D3;
const int LED_UV = D6;

// fan pin
const int FAN_PIN = D10;

// all LEDs in the order their flashed
const int LEDS[] = {LED_RED, LED_GREEN, LED_BLUE, LED_IR, LED_UV};
const char* LED_NAMES[] = {"red", "green", "blue", "ir", "uv"};
const int NUM_LEDS = 5;

//this is the light sensor above the vial
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// one reading per LED
uint16_t readings [NUM_LEDS];

// the OLED screen (128 x 64 pixels) i.e. the owls face
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// owl moods (how the eyes look)
const int AWAKE = 0;
const int SLEEPY = 1;
const int HAPPY = 2;
const int WORRIED = 3;
const int UNSURE = 4;
const int BROKEN = 5;
const int LOOK_DOWN = 6;

// where the eyes sit on the screen
const int EYE_L = 36;
const int EYE_R = 92;
const int EYE_Y = 24;
const int EYE_SIZE = 16;

// draws one eye at x in the given mood
void drawEye(int x, int mood) {
  if (mood == BROKEN) {
    // X eye
    for (int t = 0; t< 2; t++) {
      display.drawLine(x-10 + t, EYE_Y - 10, x +10 + t, EYE_Y + 10, SSD1306_WHITE);
      display.drawLine(x-10+t, EYE_Y +10, x+10+t, EYE_Y -10, SSD1306_WHITE);
    }
    return;
  }

  if (mood == HAPPY) {
    // ^ shape: a white circle with a black circle covering its bottom
    display.fillCircle(x, EYE_Y + 4, EYE_SIZE, SSD1306_WHITE);
    display.fillCircle(x, EYE_Y + 12, EYE_SIZE, SSD1306_BLACK);
    return;
  }

  // normal eye: white circle + black pupil
  display.fillCircle(x, EYE_Y, EYE_SIZE, SSD1306_WHITE);
  int pupilY = EYE_Y;
  if (mood == LOOK_DOWN) {
    pupilY = EYE_Y + 7;
  }
  display.fillCircle(x, pupilY, 6, SSD1306_BLACK);

  if (mood == SLEEPY) {
    // eyelid: black box over the top half of the eye
    display.fillRect(x - EYE_SIZE, EYE_Y - EYE_SIZE, EYE_SIZE * 2 + 1, EYE_SIZE, SSD1306_BLACK);
  }
}

// draw both eyes plus a status line at the bottom
void drawOwl(int mood, const char* status) {
  display.clearDisplay();

  if (mood == UNSURE) {
    // one eye open
    drawEye(EYE_L, AWAKE);   
    // one squinty eye
    drawEye(EYE_R, SLEEPY);  
  } else {
    drawEye(EYE_L, mood);
    drawEye(EYE_R, mood);
  }

  if (mood == WORRIED) {
    // eyebrows tilted up in the middle
    display.drawLine(EYE_L - 14, 6, EYE_L + 12, 1, SSD1306_WHITE);
    display.drawLine(EYE_R - 12, 1, EYE_R + 14, 6, SSD1306_WHITE);
  }

  // status text, centered (each letter is 6 pixels wide)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int textWidth = strlen(status) * 6;
  display.setCursor((128 - textWidth) / 2, 54);
  display.print(status);

  display.display();
}

// show each color's reading as a line of text
void showReadings() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Veritas results");
  for (int i = 0; i < NUM_LEDS; i++) {
    display.print(LED_NAMES[i]);
    display.print(": ");
    display.println(readings[i]);
  }
  display.display();
}

// bluetooth: the names the iPhone app looks for and connects to
#define DEVICE_NAME  "Veritas"
#define SERVICE_UUID "7a1c0001-3f2b-4c8e-9a6d-2b5e8f1c0a01"
// owl --> phone
#define RESULTS_UUID "7a1c0002-3f2b-4c8e-9a6d-2b5e8f1c0a01" 
// phone --> owl 
#define COMMAND_UUID "7a1c0003-3f2b-4c8e-9a6d-2b5e8f1c0a01"  

// where we put the readings for the phone
BLECharacteristic* resultsChar;  
bool phoneConnected = false;
String command = "";             

// runs when the phone connects or disconnects
class ConnectionCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) {
    phoneConnected = true;
  }
  void onDisconnect(BLEServer* server) {
    phoneConnected = false;
    // let the phone find us again
    BLEDevice::startAdvertising();  
  }
};

// runs when the phone sends a command (like "SCAN")
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) {
    command = c->getValue();
  }
};

// start Bluetooth so the phone can find veritas
void startBluetooth() {
  BLEDevice::init(DEVICE_NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ConnectionCallbacks());
  BLEService* service = server->createService(SERVICE_UUID);

  resultsChar = service->createCharacteristic(RESULTS_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  resultsChar->addDescriptor(new BLE2902());

  BLECharacteristic* commandChar = service->createCharacteristic(COMMAND_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  commandChar->setCallbacks(new CommandCallbacks());

  service->start();
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
}

// turn one LED on, measures the light that gets thru the vial, then turns it off
uint16_t readLED(int i) {
  digitalWrite(LEDS[i], HIGH);
  delay(50);
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t full = lum & 0xFFFF;
  digitalWrite(LEDS[i], LOW);
  delay(100);
  return full;
}

// countdown on the owl while the vial is shaken 
void shakeCountdown() {
  for (int s = 10; s > 0; s--) {
    char msg[20];
    sprintf(msg, "Shake vial %d", s);
    drawOwl(LOOK_DOWN, msg);
    delay(1000);
  }
}

// a test func for shaking countdown
void runScan() {
  shakeCountdown();

  String message = "";
  for (int i = 0; i < NUM_LEDS; i++) {
    // owl looks down at the vial, with a status line and a progress bar
    char msg[24];
    sprintf(msg, "Scanning %s %d/%d", LED_NAMES[i], i + 1, NUM_LEDS);
    drawOwl(LOOK_DOWN, msg);
    display.drawRect(14, 44, 100, 6, SSD1306_WHITE);
    display.fillRect(14, 44, 100 * (i + 1) / NUM_LEDS, 6, SSD1306_WHITE);
    display.display();

    readings[i] = readLED(i);
    Serial.print(LED_NAMES[i]);
    Serial.print(": ");
    Serial.println(readings[i]);

    // build the text "red,green,blue,ir,uv" for the phone
    message += String(readings[i]);
    if (i < NUM_LEDS - 1) {
      message += ",";
    }
  }

  // send the numbers to the phone
  resultsChar->setValue(message);
  resultsChar->notify();
  drawOwl(AWAKE, "Sent! Check phone");
}

void setup() {
  Serial.begin(115200);

  //led outputs and off switches
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LEDS[i], OUTPUT);
    digitalWrite(LEDS[i], LOW);
  }

  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  // start the light sensor
  bool sensorOK = tsl.begin();
  if (sensorOK) {
    Serial.println("Light sensor found");
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
  } else{
    Serial.println("Light sensor NOT found, check wiring");
  }

  // start the OLED screen (address 0x3C); sleepy owl until the phone connects w bluetooth
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    if (sensorOK) {
      drawOwl(SLEEPY, "Open the Veritas app");
    } else {
      // X eyes
      drawOwl(BROKEN, "Sensor missing");  
    }
  } else {
    Serial.println("Screen NOT found, check wiring");
  }

  startBluetooth();
  Serial.println("Veritas ready");
}

void loop() {
  // The owl reacts when the phone connects or leaves
  static bool wasConnected = false;
  if (phoneConnected != wasConnected) {
    wasConnected = phoneConnected;
    if (phoneConnected) {
      drawOwl(AWAKE, "Connected");
    } else {
      drawOwl(SLEEPY, "Open the Veritas app");
    }
  }

  // for testing without the app: type a command in the Serial Monitor
  if (Serial.available()) {
    command = Serial.readStringUntil('\n');
    command.trim();
  }

  // User performs what the phone (or the Serial Monitor) asked
  if (command == "SCAN") {
    command = "";
    runScan();
  } else if (command == "MATCH") {
    command = "";
    drawOwl(HAPPY, "Match");
  } else if (command == "NOMATCH") {
    command = "";
    drawOwl(WORRIED, "Doesn't match");
  } else if (command == "UNSURE") {
    command = "";
    drawOwl(UNSURE, "Inconclusive ?");
  } else if (command != "") {
    command = "";  
  }

  delay(50);
}