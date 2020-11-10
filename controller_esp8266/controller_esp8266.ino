#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include "Credentials.h"

const int HEATER_PIN = 2;
const int MANUAL_HEATING_TIMER_INTERVAL = 1000 * 2;
const int MANUAL_HEATING_TIME = 1000 * 60 * 3;
const int HEATING_STATUS_INTERVAL = 1000 * 60;
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

float actualTemp = 0.0;
float dayTemp = 0.0;
float nightTemp = 0.0;

BlynkTimer manualHeatingTimer;
int manualHeatingTimerID;

void setup()
{
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
    if(heatingState){
      digitalWrite(HEATER_PIN, LOW); 
    } else {
      digitalWrite(HEATER_PIN, HIGH);
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

void startManualHeating() {
  
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

BLYNK_WRITE(V10) {
  int value = param.asInt();
  if(value == 1){
    Serial.println("Updating dayState: DAY");
    dayState = DAY;
  } else {
    Serial.println("Updating dayState: NIGHT");
    dayState = NIGHT;
  }
}

BLYNK_WRITE(V1) {
  float temp = param.asFloat();
  Serial.print("Actual temperature: ");
  Serial.print(temp);
  Serial.println(" Celcius");
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
    //checkUpdates();
  }
}

/*
void checkUpdates() {
  String mac = getMAC();
  String fwURL = String( fwUrlBase );
  fwURL.concat( mac );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" );

  Serial.println( "Checking for firmware updates." );
  Serial.print( "MAC address: " );
  Serial.println( mac );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );

  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();

    Serial.print( "Current firmware version: " );
    Serial.println( FW_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if( newVersion > FW_VERSION ) {
      Serial.println( "Preparing to update" );

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  }
  httpClient.end();
}
*/
