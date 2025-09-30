#include <WiFi.h>
#include <WebServer.h>
#include "ACS712.h"
#include "ZMPT101B.h"
#include <EEPROM.h>

// WiFi credentials
const char* ssid     = "rio";
const char* password = "1234567890";

// Web server on port 80
WebServer server(80);

// Sensor setup (use valid ADC pins for ESP32-C6)
ACS712 ACS(4, 3.3, 4095, 125);        // GPIO4 for ACS712
ZMPT101B voltageSensor(5, 50.0);      // GPIO5 for ZMPT101B

#define EEPROM_SIZE   512
#define UNIT_ADDRESS  0
#define RELAY_PIN     9   // GPIO9 for relay control

float unit;
int volt, current, power;
bool relayState = false;
unsigned long lastReadTime = 0;

// HTML page generator
String getHTMLPage() {
  String relayLabel = relayState ? "ON" : "OFF";
  String buttonLabel = relayState ? "Turn OFF" : "Turn ON";

  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='1'>";
  html += "<title>Energy Monitor</title></head><body>";
  html += "<h2>ESP32-C6 Energy Monitoring</h2>";
  html += "<p><strong>Voltage:</strong> " + String(volt) + " V</p>";
  html += "<p><strong>Current:</strong> " + String(current) + " mA</p>";
  html += "<p><strong>Power:</strong> " + String(power) + " W</p>";
  html += "<p><strong>Energy:</strong> " + String(unit, 4) + " kWh</p>";
  html += "<hr>";
  html += "<p><strong>Relay State (GPIO9):</strong> " + relayLabel + "</p>";
  html += "<form action='/toggle' method='POST'>";
  html += "<button type='submit'>" + buttonLabel + "</button>";
  html += "</form>";
  html += "</body></html>";
  return html;
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Initial state OFF

  // WiFi connection
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // Load unit from EEPROM
  unit = EEPROM.readFloat(UNIT_ADDRESS);
  if (isnan(unit)) unit = 0.0;

  // Sensor setup
  ACS.autoMidPoint();
  voltageSensor.setSensitivity(700.0f);

  // Web routes
  server.on("/", []() {
    server.send(200, "text/html", getHTMLPage());
  });
  server.on("/toggle", HTTP_POST, []() {
    relayState = !relayState;
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.begin();
}

void loop() {
  server.handleClient();

  // Read values every 0.5s
  if (millis() - lastReadTime > 500) {
    float avgCurrent = 0;
    for (int i = 0; i < 100; i++) {
      avgCurrent += ACS.mA_AC();
    }
    float mA = avgCurrent / 400.0;
    current = (mA > 10) ? mA : 0;

    float voltage = voltageSensor.getRmsVoltage();
    volt = (voltage > 50) ? voltage : 0;

    float watt = volt * (mA / 1000.0);
    power = watt;

    float kWh = (watt * 0.5) / 3600000.0;  // 0.5s interval
    unit += kWh;

    static unsigned long lastEEPROM = 0;
    if (millis() - lastEEPROM > 60000) {
      EEPROM.writeFloat(UNIT_ADDRESS, unit);
      EEPROM.commit();
      lastEEPROM = millis();
    }
    lastReadTime = millis();
  }
}
