#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

const char *ssid = "*************";         // Wifi ssid
const char *password = "*********"; // Wifi password
const char *hostname = "*********";      // Display hostname

void setup()
{
  Serial.begin(115200); // Serial debug
  delay(1000);
  Serial.printf("Connecting to [%s]", ssid);
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println(" -> Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname(hostname);
  Serial.println(" -> WiFi connected!");
  Serial.printf("IP address: [%s]", WiFi.localIP().toString());
  Serial.println();
  Serial.printf("Hostname: [%s]", WiFi.getHostname());
  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
}

void loop()
{
  ArduinoOTA.handle();
}
