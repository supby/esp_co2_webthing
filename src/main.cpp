#define LARGE_JSON_BUFFERS 1
#define ARDUINOJSON_USE_LONG_LONG 1
#define I2C_ADDR = 0x62

#include <Ticker.h>
#include <Arduino.h>
#include <ArduinoJson.h>

// SCD40
#include <Wire.h>
#include "SparkFun_SCD4x_Arduino_Library.h"

// webthings
#include <WebThingAdapter.h>
#include <Thing.h>

// Network
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

// OTA
// #include <ESP8266HTTPClient.h>
// #include <ESP8266httpUpdate.h>

#include "settings.h"



WebThingAdapter* adapter;
const char *airqTypes[] = {"AirQualitySensor", nullptr};
ThingDevice airqSensor(DEVICE_NAME, DEVICE_NAME, airqTypes);
ThingProperty co2Prop("CO2", "CO2 ppm", NUMBER, "ConcentrationProperty");
ThingProperty temperatureProp("Temperature", "Temperature", NUMBER, "TemperatureProperty");
ThingProperty humidityProp("Humidity", "Humidity", NUMBER, "LevelProperty");

Ticker checkPropTimer;

SCD4x scd40sensor;

void setupSCD40() {
  Wire.begin();  

  if (scd40sensor.begin() == false)
  {
    Serial.println(F("Sensor not detected. Please check wiring. Freezing..."));
    while (1)
      ;
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
  if (scd40sensor.readMeasurement())
  {
    uint16_t co2 = scd40sensor.getCO2();
    float temperature = scd40sensor.getTemperature();
    float humidity = scd40sensor.getHumidity();

    Serial.println();

    Serial.print(F("CO2(ppm):"));
    Serial.print(co2);

    Serial.print(F("\tTemperature(C):"));
    Serial.print(temperature, 1);

    Serial.print(F("\tHumidity(%RH):"));
    Serial.print(humidity, 1);

    Serial.println();

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
  else
    Serial.print(F("failed to read measurement"));
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
  
  checkPropTimer.attach(30, checkProp);
}

void loop() {
  // readSCD40Data();
  // delay(3000);
}