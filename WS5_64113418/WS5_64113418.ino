#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* WifiSSID = "RAKSACHAT_NB 4860";
const char* WifiPassword = "123456789";

const char* ServerUrl = "http://192.168.137.1:3000/senserc";

DHT dhtSensor(D4, DHT11);

// NTP setup
const long TimeOffset = 25200; // 7 hours for Thailand
const int NTPInterval = 60000; // Update interval in milliseconds
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", TimeOffset, NTPInterval);

// HTTP client
WiFiClient wifiClient;
HTTPClient httpClient;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WifiSSID, WifiPassword);
  dhtSensor.begin();
  ntpClient.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void loop() {
  static unsigned long lastUpdateTime = 0;
  const unsigned long UpdateDelay = 15000;

  if ((millis() - lastUpdateTime) > UpdateDelay) {
    ntpClient.update();

    float humidity = dhtSensor.readHumidity();
    float temperature = dhtSensor.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor");
    } else {
      // Log readings to the serial
      Serial.printf("Humidity: %.2f%%\n", humidity);
      Serial.printf("Temperature: %.2f°C\n", temperature);

      // Get current time formatted as a string
      time_t currentTime = ntpClient.getEpochTime();
      struct tm* timeInfo = gmtime(&currentTime);
      char timeString[25];
      strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeInfo);

      // Prepare JSON document
      StaticJsonDocument<300> doc;
      JsonObject dataObject = doc.createNestedObject(timeString);
      dataObject["humidity"] = humidity;
      dataObject["temperature"] = temperature;

      // Serialize and send the JSON document
      String jsonPayload;
      serializeJson(doc, jsonPayload);
      httpClient.begin(wifiClient, ServerUrl);
      httpClient.addHeader("Content-Type", "application/json");
      int httpResponseCode = httpClient.PATCH(jsonPayload);

      if (httpResponseCode > 0) {
        String responsePayload = httpClient.getString();
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        Serial.println("Returned payload:");
        Serial.println(responsePayload);
      } else {
        Serial.printf("Error on sending PATCH: %d\n", httpResponseCode);
      }

      httpClient.end();
    }

    lastUpdateTime = millis();
  }
}
