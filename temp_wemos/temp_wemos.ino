#define BLYNK_PRINT Serial
#define ONE_WIRE_PIN D2

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "credentials.h"

int TEMPERATURE_UPDATE_INTERVAL = 1000 * 60;

WidgetBridge espBridge(V1);
BlynkTimer updateTempTimer;
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(9600);
  Blynk.begin(AUTH, NETWORK, PASS);
  updateTempTimer.setInterval(TEMPERATURE_UPDATE_INTERVAL, updateTemperature);
}

void updateTemperature() {
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);  
  if(tempC == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: Could not read temperature data");
    return;
  }
  Serial.print("Temp C: ");
  Serial.println(tempC);
  Blynk.virtualWrite(V1, tempC);
  espBridge.virtualWrite(V1, tempC);
}

void loop() {
  Blynk.run();
  updateTempTimer.run();
}

BLYNK_CONNECTED() {
  espBridge.setAuthToken("3st6yhPshCNEfrmlhOtkVXa-AlZohhmJ");
}
