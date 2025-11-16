#include <Wire.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

// ===== Pin Configuration =====
#define DHTPIN 2
#define DHTTYPE DHT11
#define ONE_WIRE_BUS 9
#define RELAY1_PIN 3
#define RELAY2_PIN 4
#define CS_PIN 10     // SD card chip select
#define DS1302_CLK 5  // SCK
#define DS1302_DAT 6  // I/O
#define DS1302_RST 7  // RST

const int BTN_UP = A0;
const int BTN_DOWN = A1;
const int BTN_SELECT = A2;

// ===== Objects =====
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 4);  // Change 0x27 to 0x3F if needed
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1, sensor2;  // Two DS18B20 sensors
ThreeWire myWire(DS1302_DAT, DS1302_CLK, DS1302_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

#define TEMPERATURE_PRECISION 9

// ===== Variables =====
float airTemp = 0, humidity = 0;
float waterTemp1 = 0, waterTemp2 = 0;
int setTemperature = 70;
bool adjustMode = false;

unsigned long lastSave = 0;         // for periodic SD logging
unsigned long logInterval = 60000;  // 1 minute

bool lastButtonState = HIGH;

//==================================================================================
// function to print a device address (debug)
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

//==================================================================================
String getTime() {
  char timeStr[10];
  RtcDateTime now = Rtc.GetDateTime();
  sprintf(timeStr, "%02u:%02u:%02u", now.Hour(), now.Minute(), now.Second());
  // Serial.println(timeStr);
  return String(timeStr);
}

String getDate() {
  char dateStr[12];
  RtcDateTime now = Rtc.GetDateTime();
  sprintf(dateStr, "%02u/%02u/%04u", now.Day(), now.Month(), now.Year());
  // Serial.println(dateStr);
  return String(dateStr);
}

//==================================================================================
// Function to adjust temperature
void adjustTemperature() {
  if (digitalRead(BTN_UP) == LOW) {
    setTemperature++;
    delay(200);  // debounce
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    setTemperature--;
    delay(200);  // debounce
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Adjust Temp:");
  lcd.setCursor(0, 1);
  lcd.print(setTemperature);
  lcd.print((char)223);
  lcd.print("C");
}

//==================================================================================
// Function to update LCD based on mode
void updateLCD() {
  if (adjustMode) {
    adjustTemperature();
  } else {
    displayData();
  }
}

//==================================================================================
void relayControl() {
  if (waterTemp1 < setTemperature) {
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);
  } else {
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
  }
}

void storeData() {
  if (!SD.begin(CS_PIN)) {
    Serial.println("SD card not detected.");
    return;
  }
  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.print(getDate());
    dataFile.print(", ");
    dataFile.print(getTime());
    dataFile.print(", ");
    dataFile.print(airTemp);
    dataFile.print(", ");
    dataFile.print(humidity);
    dataFile.print(", ");
    dataFile.print(waterTemp1);
    dataFile.print(", ");
    dataFile.println(waterTemp2);
    dataFile.close();
    Serial.println("Data saved to SD card.");
  } else {
    Serial.println("Error opening file on SD card.");
  }
}

// ===== Read All Sensors =====
void readSensors() {
  airTemp = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(airTemp) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    // return;
  }

  sensors.requestTemperatures();
  sensors.getAddress(sensor1, 0);
  sensors.getAddress(sensor2, 1);

  waterTemp1 = sensors.getTempC(sensor1);
  waterTemp2 = sensors.getTempC(sensor2);

  Serial.print("AT:");
  Serial.print(airTemp);
  Serial.print("C ");
  Serial.print("H:");
  Serial.print(humidity);
  Serial.print("% ");
  Serial.print("W1:");
  Serial.print(waterTemp1);
  Serial.print("C ");
  Serial.print("W2:");
  Serial.print(waterTemp2);
  Serial.println("C");
}

// ===== LCD Display =====
void displayData() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AT:");
  lcd.print(airTemp, 1);
  lcd.print("C H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("W1:");
  lcd.print(waterTemp1, 1);
  lcd.print("C W2:");
  lcd.print(waterTemp2, 1);
  lcd.print("C");

  lcd.setCursor(0, 2);
  lcd.print(getTime());

  lcd.setCursor(0, 3);
  lcd.print(getDate());
}

//==================================================================================
void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  dht.begin();
  sensors.begin();
  Rtc.Begin();

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  int numberOfDevices = sensors.getDeviceCount();
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" DS18B20 devices.");
}

//==================================================================================
void loop() {
  String date = getDate();
  String time = getTime();
  Serial.println(date);
  Serial.println(time);
  getTime();
  readSensors();
  relayControl();
  updateLCD();
  storeData();
  // Save to SD every logInterval
  if (millis() - lastSave > logInterval) {
    storeData();
    lastSave = millis();
  }

  // Check for mode toggle (SELECT button)
  bool buttonState = digitalRead(BTN_SELECT);
  if (lastButtonState == HIGH && buttonState == LOW) {
    adjustMode = !adjustMode;  // toggle between adjust & display
    delay(200);
  }
  lastButtonState = buttonState;

  delay(500);
}
