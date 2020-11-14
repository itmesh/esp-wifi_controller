#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include "Credentials.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
WidgetTerminal terminal(V23);
WidgetLED dayModeLed(V25);
WidgetLED nightModeLed(V24);

const int LED_PIN = 2;
const int HEATER_PIN = 14;
const int MANUAL_HEATING_TIMER_INTERVAL = 1000 * 2; // 2 seconds
const int MANUAL_HEATING_TIME = 1000 * 60 * 3; // 3 minutes
const int HEATING_STATUS_INTERVAL = 1000 * 60; // 1 minute
const int PROGRESS_BAR_MIN = 0;
const int PROGRESS_BAR_MAX = 100;
const int PROGRESS_BAR_STEP =  PROGRESS_BAR_MAX / (MANUAL_HEATING_TIME / MANUAL_HEATING_TIMER_INTERVAL);
const float MIN_TEMP_VALID = 15.0;
const float MAX_TEMP_VALID = 25.0;

bool initialHeating = true;

int criticalError = false;
int currentBarProgress = PROGRESS_BAR_MAX;
enum timeOfTheDay {DAY, NIGHT};
timeOfTheDay dayState = DAY;
bool manualHeating = false;

int heatingState = 0;
int autoHeatingState = 0;
int manualHeatingState = 0;

float displayingTemp = 0.0;
float actualTemp = 0.0;
float dayTemp = 0.0;
float nightTemp = 0.0;

BlynkTimer manualHeatingTimer;
int manualHeatingTimerID;

void setup()
{
  Wire.begin(D2, D1);
  lcd.begin();
  lcd.home();
  
  Serial.begin(9600);
  Serial.println("Initializing...");
  Serial.print("PROGRESS_BAR_STEP: ");
  Serial.println(PROGRESS_BAR_STEP);
  Serial.print("MANUAL_HEATING_TIME: ");
  Serial.println(MANUAL_HEATING_TIME);
  Blynk.begin(AUTH, NETWORK, PASS);
  while (Blynk.connect() == false) {}
  pinMode(HEATER_PIN, OUTPUT);
  initManualHeating();
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V10);
  Blynk.syncVirtual(V21);
  Blynk.syncVirtual(V22);
  updateHeatingState();
  manualHeatingTimer.setInterval(HEATING_STATUS_INTERVAL, updateHeatingStatus);
  initialHeating = false;
}

void loop()
{
  Blynk.run();
  manualHeatingTimer.run();
  updateHeatingState();
}

void initManualHeating() {
  manualHeatingTimerID = manualHeatingTimer.setInterval(MANUAL_HEATING_TIMER_INTERVAL, updateProgressBarStatus);
  stopManualHeating();
}

void updateHeatingStatus() {
  Blynk.virtualWrite(V15, heatingState);
}

void updateHeatingState() {
  int previousHeatingState = heatingState;
  if(dayState == DAY) {
    /*
    Serial.println("Checking day temperature...");
    Serial.print("  actualTemp: ");
    Serial.println(actualTemp);
    Serial.print("  dayTemp: ");
    Serial.println(dayTemp);
    Serial.println();
    */
    if(actualTemp >= dayTemp) {
      autoHeatingState = 0; 
    } else {
      autoHeatingState = 1; 
    }
    if(displayingTemp != dayTemp) {
      displayTargetTemp(dayTemp);
    }
  } else if(dayState == NIGHT) {
    /*
    Serial.println("Checking night temperature...");
    Serial.print("  actualTemp: ");
    Serial.println(actualTemp);
    Serial.print("  nightTemp: ");
    Serial.println(nightTemp);
    Serial.println();
    */
    if(actualTemp >= nightTemp) {
      autoHeatingState = 0; 
    } else {
      autoHeatingState = 1; 
    }
    if(displayingTemp != nightTemp) {
      displayTargetTemp(nightTemp);
    }
  }
  /*
  Serial.print("heatingState: ");
  Serial.println(heatingState);
  Serial.print("manualHeatingState: ");
  Serial.println(manualHeatingState);
  Serial.print("autoHeatingState: ");
  Serial.println(autoHeatingState);
  */
  if(criticalError) {
    heatingState = 0;
  } else if(manualHeating) {
    heatingState = manualHeatingState;
  } else {
    heatingState = autoHeatingState;
  }
  if(previousHeatingState != heatingState || initialHeating) {
    Blynk.virtualWrite(V20, heatingState);
    displayHeaterState(heatingState);
    if(heatingState){
      digitalWrite(LED_PIN, LOW); 
      digitalWrite(HEATER_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(HEATER_PIN, LOW);
    }
  }
}

void updateProgressBarStatus() {
  currentBarProgress = currentBarProgress - PROGRESS_BAR_STEP;
  Serial.print("Bar satatus - time left: ");
  Serial.println(currentBarProgress);
  if(currentBarProgress > PROGRESS_BAR_MIN) {
    Serial.print("Updating progress bar status: ");
    Serial.print(currentBarProgress);
    Serial.print("/");
    Serial.println(PROGRESS_BAR_MAX);
    Blynk.virtualWrite(V11, currentBarProgress);
  } else {
    Serial.println("Manual heating finishing...");
    stopManualHeating();
  }
}

void timerCheck() {
  Serial.println("Timer check...");
}

void stopManualHeating() {
  Serial.println("Stop manual heating");
  if(manualHeatingTimer.isEnabled(manualHeatingTimerID)){
    Serial.println("Disable manual heating progress bar");
    manualHeatingTimer.disable(manualHeatingTimerID);
  }
  manualHeating = false;
  currentBarProgress = PROGRESS_BAR_MAX;
  Blynk.virtualWrite(V11, PROGRESS_BAR_MIN);
}

void displayHeaterState(int state) { 
  lcd.setCursor(0,0);
  lcd.printf("S: %d", state);
}

void displayTargetTemp(float temp){
  displayingTemp = temp;
  lcd.setCursor(8,0);
  lcd.printf("T: %.2f", temp);
  lcd.print((char)223);
  lcd.print(" ");
}

void displayRoomTemp(float temp) {
  lcd.setCursor(0,1);
  lcd.printf("Room T: %.2f", temp);
  lcd.print((char)223);
  lcd.print(" ");
}

void setDayState(int value) {
  if(value == 1){
    Serial.println("Updating dayState: DAY");
    dayState = DAY;
    dayModeLed.on();
    nightModeLed.off();
  } else {
    Serial.println("Updating dayState: NIGHT");
    dayState = NIGHT;
    dayModeLed.off();
    nightModeLed.on();
  }
}

// Heating button
BLYNK_WRITE(V20) {
  int value = param.asInt();
  Serial.print("Setting manual heating state: ");
  Serial.println(value);
  manualHeatingState = value;
  if(manualHeatingState != autoHeatingState) {
    manualHeating = true;
    manualHeatingTimer.enable(manualHeatingTimerID);
    manualHeatingTimer.restartTimer(manualHeatingTimerID);
    Blynk.virtualWrite(V11, PROGRESS_BAR_MAX);
  } else {
    stopManualHeating();
  }
}

// Manual heating reset
BLYNK_WRITE(V12) {
  int value = param.asInt();
  if(value == 1) {
    if(manualHeatingTimer.isEnabled(manualHeatingTimerID)){
      manualHeatingTimer.restartTimer(manualHeatingTimerID);
      currentBarProgress = PROGRESS_BAR_MAX;
      Blynk.virtualWrite(V11, PROGRESS_BAR_MAX);
      Serial.println("Reseting manual heating timer...");
    } else {
      Serial.println("Manual heating timer doesn't run!");
    }
  }
}

// Manual heating stop
BLYNK_WRITE(V13) {
  int value = param.asInt();
  if(value == 1) {
    Serial.println("Stopping manual heating...");
    stopManualHeating();
  }
}

// Setting day state from eventor
BLYNK_WRITE(V10) {
  int value = param.asInt();
  setDayState(value);
}

// Setting day state to night state from button
BLYNK_WRITE(V14) {
  int value = param.asInt();
  if(value == 1){
    setDayState(0);
  }
}

// Setting day state to day state from button
BLYNK_WRITE(V15) {
  int value = param.asInt();
  if(value == 1){
    setDayState(1);
  }
}

BLYNK_WRITE(V1) {
  float temp = param.asFloat();
  Serial.print("Actual temperature: ");
  Serial.print(temp);
  Serial.println(" Celcius");
  displayRoomTemp(temp);
  if(MIN_TEMP_VALID <= temp && temp <= MAX_TEMP_VALID) {
    actualTemp = temp;
    Serial.println("Temperature updated");
  }
}

BLYNK_WRITE(V21) {
  float temp = param.asFloat();
  Serial.print("Day temperature: ");
  Serial.print(temp);
  Serial.println(" Celcius");
  if(MIN_TEMP_VALID <= temp && temp <= MAX_TEMP_VALID) {
    dayTemp = temp;
    Serial.println("Temperature updated");
  }
}

BLYNK_WRITE(V22) {
  float temp = param.asFloat();
  Serial.print("Night temperature: ");
  Serial.print(temp);
  Serial.println(" Celcius");
  if(MIN_TEMP_VALID <= temp && temp <= MAX_TEMP_VALID) {
    nightTemp = temp;
    Serial.println("Temperature updated");
  }
}

BLYNK_WRITE(V17) {
  if(param.asInt() == 1) {
    terminal.println("Updating is not implemented yet.");
    terminal.flush();
    /*
    WiFiClient client;
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, "http://192.168.8.116:8080/file.bin");
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
    */
  }
}

BLYNK_WRITE(V23)
{
  String cmd = param.asStr();
  Serial.println(cmd);
  if(cmd.indexOf("blynk") == 0){
    if(cmd.length() < 7) {
      terminal.println("Add some command.");
      terminal.flush();
    } else if(cmd.indexOf("clear") > 0){
      terminal.clear();
      terminal.flush();      
    }else if(cmd.indexOf("update") > 0){
      terminal.println("Updating is not implemented yet.");
      terminal.flush();
    } else {
      terminal.print("Command '");
      terminal.print(cmd.substring(6));
      terminal.println("' not found.");
      terminal.flush();
    }
  } else {
    terminal.println("Command not found, start with 'blynk'");
    terminal.flush();
  }
}
