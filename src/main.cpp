#define LARGE_JSON_BUFFERS 1
#define ARDUINOJSON_USE_LONG_LONG 1

#include <Ticker.h>
#include <Arduino.h>
#include <ArduinoJson.h>

// SCD40
#include <Wire.h>
#include <SensirionCore.h>
#include <SensirionI2CScd4x.h>

// webthings
#include <WebThingAdapter.h>
#include <Thing.h>

// Network
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

// OTA
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include "settings.h"

WebThingAdapter* adapter;
const char *airqTypes[] = {"AirQualitySensor", nullptr};
ThingDevice airqSensor("airq", DEVICE_NAME, airqTypes);
ThingProperty co2Prop("CO2", "CO2 ppm", NUMBER, "ConcentrationProperty");
ThingProperty temperatureProp("Temperature", "Temperature", NUMBER, "TemperatureProperty");
ThingProperty humidityProp("Humidity", "Humidity", NUMBER, "LevelProperty");

Ticker checkPropTimer;

SensirionI2CScd4x scd4x;

void printUint16Hex(uint16_t value) {
    Serial.print(value < 4096 ? "0" : "");
    Serial.print(value < 256 ? "0" : "");
    Serial.print(value < 16 ? "0" : "");
    Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
    Serial.print("Serial: 0x");
    printUint16Hex(serial0);
    printUint16Hex(serial1);
    printUint16Hex(serial2);
    Serial.println();
}

void setupSCD40() {
  Wire.begin();

  uint16_t error;
  char errorMessage[256];

  scd4x.begin(Wire);

  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
      Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  }

  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
      Serial.print("Error trying to execute getSerialNumber(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  } else {
      printSerialNumber(serial0, serial1, serial2);
  }

  // Start Measurement
  error = scd4x.startPeriodicMeasurement();
  if (error) {
      Serial.print("Error trying to execute startPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  }
}

void setupWebThing(String deviceName) {
  
  co2Prop.readOnly = true;
  co2Prop.title = "CO2";

  temperatureProp.readOnly = true;
  temperatureProp.title = "Temperature";
  

  humidityProp.readOnly = true;
  humidityProp.unit = "percent";
  humidityProp.title = "Humidity";
  humidityProp.minimum = 0;
  humidityProp.maximum = 100;  

  adapter = new WebThingAdapter(deviceName, WiFi.localIP());
  
  airqSensor.addProperty(&co2Prop);
  airqSensor.addProperty(&temperatureProp);
  airqSensor.addProperty(&humidityProp);
  
  adapter->addDevice(&airqSensor);
  adapter->begin();
}

void setupWiFi(String deviceName) {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(deviceName.c_str());
  WiFi.setAutoReconnect(true);

  bool blink = true;
  if (WiFi.SSID() == "") {
    WiFi.beginSmartConfig();

    while (!WiFi.smartConfigDone()) {
      delay(1000);
      digitalWrite(LED_PIN, blink ? HIGH : LOW);
      blink = !blink;
    }
  }

  WiFi.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_PIN, blink ? HIGH : LOW);
    blink = !blink;
  }

  WiFi.stopSmartConfig();

  digitalWrite(LED_BUILTIN, HIGH);
}

void readSCD40Data() {
  uint16_t error;
  char errorMessage[256];
  
  uint16_t co2 = 0;
  float temperature = 0.0f;
  float humidity = 0.0f;
  uint16_t dataReady = false;
  error = scd4x.getDataReadyStatus(dataReady);
  if (error) {
      Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      return;
  }
  
  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error) {
      Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      return;
  }

  if (co2 == 0) {
      Serial.println("Invalid sample detected, skipping.");
      return;
  }

  Serial.print("Co2:");
  Serial.print(co2);
  Serial.print("\t");
  Serial.print("Temperature:");
  Serial.print(temperature);
  Serial.print("\t");
  Serial.print("Humidity:");
  Serial.println(humidity);

  if (temperature != temperatureProp.getValue().number) {
    ThingPropertyValue value;
    value.number = temperature;
    temperatureProp.setValue(value);
  }
  
  if (humidity != humidityProp.getValue().number) {
    ThingPropertyValue value;
    value.number = humidity;
    humidityProp.setValue(value);
  }

  if (co2 != co2Prop.getValue().number) {
    ThingPropertyValue value;
    value.number = co2;
    co2Prop.setValue(value);
  }
}

void checkProp() {
  
  MDNS.update();

  readSCD40Data();
  
  adapter->update();
}

void setup() {

  digitalWrite(LED_PIN, LED_ON);
  
  Serial.begin(115200);
  Serial.println("\n");

  // outputs  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  String deviceName(DEVICE_NAME);
  deviceName.concat("-");
  deviceName.concat(ESP.getChipId());
  deviceName.toLowerCase();
  
  setupWiFi(deviceName);

  Serial.print("OK");
  Serial.println("");

  digitalWrite(LED_PIN, LED_OFF);

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  setupWebThing(deviceName);

  setupSCD40();
  
  checkPropTimer.attach(1, checkProp);
}

void loop() {
  
}