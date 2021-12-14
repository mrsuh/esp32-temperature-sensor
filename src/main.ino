#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <WiFi.h>

#define PID_TEMPERATURE_SENSOR 33
#define PID_VIN 12
#define PID_LED 19

#define THINGSPEAK_TOKEN "token"
#define THINGSPEAK_FIELD "field1"

#define SLEEP_SECONDS 3600
#define WAKEUP_SECONDS 60

const char *networkSsid = "name";
const char *networkPassword = "password";

OneWire oneWire(PID_TEMPERATURE_SENSOR);
DallasTemperature sensor(&oneWire);

boolean wifiConnected = false;
float temperature = 0.0;
int initTimestamp = 0;

void switchWifiConnectedLedError(bool enabled) {
  if (enabled) {
    digitalWrite(PID_LED, HIGH);
  } else {
    digitalWrite(PID_LED, LOW);
  }
}

void connectToWiFi(const char *ssid, const char *pwd) {
  Serial.printf("Connecting to WiFi network: %s\n", ssid);

  // delete old config
  WiFi.disconnect(true);

  WiFi.onEvent(WiFiEvent);

  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

// https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial.print("WiFi connected!");
    wifiConnected = true;
    switchWifiConnectedLedError(false);
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    wifiConnected = false;
    switchWifiConnectedLedError(true);
    WiFi.reconnect();
    break;
  default:
    break;
  }
}

void lebBlinkOnSwithOn() {
  esp_sleep_wakeup_cause_t wakeupReason;

  wakeupReason = esp_sleep_get_wakeup_cause();

  switch (wakeupReason) {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeupReason);
    for (int i = 0; i < 4; i++) {
      switchWifiConnectedLedError(true);
      delay(250);
      switchWifiConnectedLedError(false);
      delay(250);
    }

    break;
  }
}

void deepSleep() {
  Serial.println("Deep sleep start");
  esp_sleep_enable_timer_wakeup(1ULL * SLEEP_SECONDS * 1000 * 1000);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  lebBlinkOnSwithOn();

  pinMode(PID_VIN, OUTPUT);
  pinMode(PID_LED, OUTPUT);
  digitalWrite(PID_VIN, HIGH);
  digitalWrite(PID_LED, LOW);

  sensor.begin();
  sensor.setResolution(12);

  connectToWiFi(networkSsid, networkPassword);

  initTimestamp = millis();

  Serial.print("Initialized\n");
}

void loop() {
  if (millis() - initTimestamp > (WAKEUP_SECONDS * 1000)) {
    deepSleep();
    return;
  }

  if (!wifiConnected) {
    return;
  }

  sensor.requestTemperatures();
  temperature = sensor.getTempCByIndex(0);
  Serial.printf("Sensor temperature: %f\n", temperature);

  if (temperature == 85.0) { // default sensor value
    Serial.println("Sensor invalid temperature");
    return;
  }

  HTTPClient http;

  String url =
      "https://api.thingspeak.com/update?api_key=" +
      String(THINGSPEAK_TOKEN) +
      "&" + String(THINGSPEAK_FIELD) +
      "=" + String(temperature, 2);

  Serial.printf("Request url: %s\n", url);

  // Your Domain name with URL path or IP address with path
  http.begin(url);

  // Send HTTP GET request
  int httpResponseCode = http.GET();
  if (httpResponseCode != 200) {
    Serial.printf("Request error code: %d\n", httpResponseCode);
    return;
  }

  // Free resources
  http.end();

  Serial.println("Request done!");

  deepSleep();
}
