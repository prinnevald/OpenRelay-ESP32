/*
  ESP32 MAC Address printout
  esp32-mac-address.ino
  Prints MAC Address to Serial Monitor

  DroneBot Workshop 2022
  https://dronebotworkshop.com
*/

// Include WiFi Library
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

void setup() {

  // Setup Serial Monitor
  Serial.begin(115200);

  // Put ESP32 into Station mode
  WiFi.mode(WIFI_AP_STA);

  // Print MAC Address to Serial monitor
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {

}
