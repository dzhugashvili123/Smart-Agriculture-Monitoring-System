#include <ESP8266WiFi.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include "DHT.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ArduinoJson.h>

namespace config {
  const char* wifiSsid = "Galaxy-M20";
  const char* wifiPassword = "ac312124";

  const char* mqttHost = "io.adafruit.com";
  const int mqttPort = 1883;
  const char* mqttUser = "aschoudhary";
  const char* mqttKey = "1ac95cb8580b4271bbb6d9f75d0668f1";

  const char* weatherHost = "api.openweathermap.org";
  const String cityQuery = "Hyderabad,IN";
  const String weatherApiKey = "e8b22b36da932dce8f31ec9be9cb68a3";

  const int ldrPin = D1;
  const int ledPin = D0;
  const int moisturePin = A0;
  const int motorPin = D8;
  const int oneWirePin = 4;  // NodeMCU D2
  const int dhtPin = D4;

  const int darkThreshold = 200;
  const float pumpOnBelow = 35.0;
  const float pumpOffAbove = 38.0;

  const unsigned long forecastPeriodMs = 10UL * 60UL * 1000UL;
  const unsigned long publishPeriodMs = 50000UL;
  const unsigned long subscriptionWaitMs = 5000UL;
  const unsigned long loopPauseMs = 9000UL;
  const unsigned long httpTimeoutMs = 5000UL;
  const int jsonCapacity = 2500;
  const uint8_t mqttAttempts = 3;
}

WiFiClient net;
Adafruit_MQTT_Client mqtt(
  &net,
  config::mqttHost,
  config::mqttPort,
  config::mqttUser,
  config::mqttKey
);

Adafruit_MQTT_Publish moistureFeed(&mqtt, config::mqttUser "/f/Moisture");
Adafruit_MQTT_Publish temperatureFeed(&mqtt, config::mqttUser "/f/Temperature");
Adafruit_MQTT_Publish humidityFeed(&mqtt, config::mqttUser "/f/Humidity");
Adafruit_MQTT_Publish soilTempFeed(&mqtt, config::mqttUser "/f/SoilTemp");
Adafruit_MQTT_Publish weatherFeed(&mqtt, config::mqttUser "/f/WeatherData");
Adafruit_MQTT_Subscribe ledFeed(&mqtt, config::mqttUser "/f/LED");
Adafruit_MQTT_Subscribe pumpFeed(&mqtt, config::mqttUser "/f/Pump");

DHT dht(config::dhtPin, DHT11);
OneWire oneWire(config::oneWirePin);
DallasTemperature soilProbe(&oneWire);

float moisturePercentage = 0;
int airTemperature = 0;
int airHumidity = 0;
int soilTemperature = 0;

String forecastBody;
const char* weatherIcon = "";
bool forecastStarted = false;
int braceBalance = 0;
int wifiStatus = WL_IDLE_STATUS;

unsigned long lastForecastAt = config::forecastPeriodMs;
unsigned long lastPublishAt = 0;

void connectWifi();
void ensureMqtt();
void refreshForecastIfDue();
void applyLightControl();
void applyMoistureControl();
void sampleClimate();
void publishSamples(unsigned long now);
void applyRemoteCommands();
void requestForecast();
void consumeForecastByte(char byteIn);
void interpretForecast(const char* json);

void setup() {
  Serial.begin(9600);
  delay(10);

  dht.begin();
  soilProbe.begin();
  mqtt.subscribe(&ledFeed);
  mqtt.subscribe(&pumpFeed);

  pinMode(config::motorPin, OUTPUT);
  pinMode(config::ledPin, OUTPUT);
  pinMode(config::ldrPin, INPUT);
  digitalWrite(config::motorPin, LOW);
  digitalWrite(config::ledPin, HIGH);

  forecastBody.reserve(config::jsonCapacity);
  connectWifi();
}

void loop() {
  const unsigned long now = millis();

  ensureMqtt();
  refreshForecastIfDue();
  applyLightControl();
  applyMoistureControl();
  sampleClimate();
  publishSamples(now);
  applyRemoteCommands();

  delay(config::loopPauseMs);
}

void connectWifi() {
  Serial.println("Connecting to ");
  Serial.println(config::wifiSsid);
  WiFi.begin(config::wifiSsid, config::wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
}

void ensureMqtt() {
  if (mqtt.connected()) {
    return;
  }

  uint8_t attemptsLeft = config::mqttAttempts;
  int8_t result = 0;

  while ((result = mqtt.connect()) != 0) {
    mqtt.disconnect();
    delay(5000);
    attemptsLeft--;
    if (attemptsLeft == 0) {
      while (1) {
      }
    }
  }
}

void refreshForecastIfDue() {
  if (millis() - lastForecastAt > config::forecastPeriodMs) {
    lastForecastAt = millis();
    requestForecast();
  }
}

void applyLightControl() {
  const int level = analogRead(config::ldrPin);

  if (level <= config::darkThreshold) {
    digitalWrite(config::ledPin, HIGH);
    Serial.print("Its DARK, Turn on the LED : ");
    Serial.println(level);
    return;
  }

  digitalWrite(config::ledPin, LOW);
  Serial.print("Its BRIGHT, Turn off the LED : ");
  Serial.println(level);
}

void applyMoistureControl() {
  const float raw = analogRead(config::moisturePin);
  moisturePercentage = 100.00 - ((raw / 1023.00) * 100.00);

  Serial.print("Soil Moisture is  = ");
  Serial.print(moisturePercentage);
  Serial.println("%");

  if (moisturePercentage < config::pumpOnBelow) {
    digitalWrite(config::motorPin, HIGH);
  }
  if (moisturePercentage > config::pumpOffAbove) {
    digitalWrite(config::motorPin, LOW);
  }
}

void sampleClimate() {
  airTemperature = dht.readTemperature();
  airHumidity = dht.readHumidity();
  soilProbe.requestTemperatures();
  soilTemperature = soilProbe.getTempCByIndex(0);
}

void publishSamples(unsigned long now) {
  if (now - lastPublishAt < config::publishPeriodMs) {
    return;
  }

  if (!moistureFeed.publish(moisturePercentage)) {
  }
  if (!temperatureFeed.publish(airTemperature)) {
  }
  if (!humidityFeed.publish(airHumidity)) {
  }
  if (!soilTempFeed.publish(soilTemperature)) {
  }
  if (!weatherFeed.publish(weatherIcon)) {
  }
  lastPublishAt = now;
}

void driveFromFeed(Adafruit_MQTT_Subscribe& feed, int pin, bool onMeansHigh) {
  Serial.println((char*)feed.lastread);
  const bool isOff = strcmp((char*)feed.lastread, "OFF") == 0;
  const bool isOn = strcmp((char*)feed.lastread, "ON") == 0;

  if (isOff) {
    digitalWrite(pin, onMeansHigh ? LOW : HIGH);
  }
  if (isOn) {
    digitalWrite(pin, onMeansHigh ? HIGH : LOW);
  }
}

void applyRemoteCommands() {
  Adafruit_MQTT_Subscribe* incoming = nullptr;

  while ((incoming = mqtt.readSubscription(config::subscriptionWaitMs))) {
    if (incoming == &ledFeed) {
      driveFromFeed(ledFeed, config::ledPin, true);
    }
    if (incoming == &pumpFeed) {
      driveFromFeed(pumpFeed, config::motorPin, false);
    }
  }
}

void requestForecast() {
  net.stop();

  if (!net.connect(config::weatherHost, 80)) {
    Serial.println("connection failed");
    return;
  }

  net.println(
    "GET /data/2.5/forecast?q=" + config::cityQuery +
    "&APPID=" + config::weatherApiKey +
    "&mode=json&units=metric&cnt=2 HTTP/1.1"
  );
  net.println("Host: api.openweathermap.org");
  net.println("User-Agent: ArduinoWiFi/1.1");
  net.println("Connection: close");
  net.println();

  const unsigned long started = millis();
  while (net.available() == 0) {
    if (millis() - started > config::httpTimeoutMs) {
      Serial.println(">>> Client Timeout !");
      net.stop();
      return;
    }
  }

  while (net.available()) {
    consumeForecastByte(net.read());
  }
}

void consumeForecastByte(char byteIn) {
  if (byteIn == '{') {
    forecastStarted = true;
    braceBalance++;
  }
  if (byteIn == '}') {
    braceBalance--;
  }
  if (forecastStarted) {
    forecastBody += byteIn;
  }
  if (braceBalance == 0 && forecastStarted) {
    interpretForecast(forecastBody.c_str());
    forecastBody = "";
    forecastStarted = false;
  }
}

const char* iconForDescription(const String& description) {
  if (description == "few clouds") {
    return "Few Clouds";
  }
  if (description == "rain") {
    return "Rain";
  }
  if (description == "broken clouds") {
    return "Broken Clouds";
  }
  return "Sunny";
}

void interpretForecast(const char* json) {
  const size_t bufferSize =
    2 * JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(2) +
    4 * JSON_OBJECT_SIZE(1) + 3 * JSON_OBJECT_SIZE(2) +
    3 * JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) +
    2 * JSON_OBJECT_SIZE(7) + 2 * JSON_OBJECT_SIZE(8) + 720;

  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  JsonArray& list = root["list"];
  JsonObject& nowT = list[0];
  JsonObject& later = list[1];
  JsonObject& tommorow = list[2];

  String city = root["city"]["name"];
  String weatherNow = nowT["weather"][0]["description"];
  String weatherLater = later["weather"][0]["description"];
  String list12 = later["weather"][0]["list"];

  Serial.println(list12);
  Serial.println(weatherLater);

  weatherIcon = iconForDescription(weatherLater);
  if (weatherLater == "few clouds" || weatherLater == "rain" || weatherLater == "broken clouds") {
    Serial.print(weatherIcon);
  }

  (void)tommorow;
  (void)city;
  (void)weatherNow;
}
