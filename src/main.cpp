#include "SparkFunCCS811.h"
#include <Arduino.h>
#include "main.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ctime>
#include <queue>
#include "time.h"
#include <string>

#define CCS811_ADDR 0x5B //Default I2C Address
//#define CCS811_ADDR 0x5A //Alternate I2C Address

const char * networkName = "test";
const char * networkPswd = "testtesttesttestTest123!_*testtest123";
const int sleepTimeInSeconds = 15;
String macAdress = "";
CCS811 mySensor(CCS811_ADDR);
int period = 60000;
unsigned long time_now = 0;
std::queue<String> myqueue;
int trysForWifiConnection = 200;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

void setup()
{
  Wire.begin();
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH); //LED on -> active
  Serial.begin(11500);
  Serial.print("Started");
  initSensor();
  tryConnectWLAN();
  synchronizeTime();
  trysForWifiConnection = 26; //Try to connect WIFI the first time longer, later just under 30sec
}

void loop()
{
  time_now = millis();
  mySensor.readAlgorithmResults();
  bool connected = tryConnectWLAN();
  String jsonString = readSensorDataToJSON(mySensor);
  //delay(1000);
  if (!(connected && sendHistoricalData() && sendActData(jsonString))) {
      persiste(jsonString);
    }
deepSleep();
  
}

void printTest() {
  Serial.print("test");
}

void initSensor() {
  for (int i = 0; i < 5; i++) {
  CCS811Core::status returnCode = mySensor.begin();
  if (returnCode != CCS811Core::SENSOR_SUCCESS)
  {
    Serial.println("begin() returned with an error. Code: ");
    Serial.println(returnCode);
    delay(1000);
  }

  }
}

/**
 * Trys to connect to WIFI-Network.
 * SSID and PW can be configured via constants networkName and networkPassword
 * @return true if WIFI-Status is connected. 
 * Execution can fail if SSID/PW not valid or connection timed out 
 */ 
bool tryConnectWLAN() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi network: " + String(networkName));

    int trys = 0;
    WiFi.begin(networkName, networkPswd);
    while (WiFi.status() != WL_CONNECTED && trys < trysForWifiConnection) 
    {
      digitalWrite(5, LOW); //let LED blink while try to connect
      delay(500);
      digitalWrite(5, HIGH);
      delay(500);

      trys++;
      Serial.print("trys:");
      Serial.println(trys);
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi Connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("timed OUT");
      Serial.print("WiFi-Status-Code: " + WiFi.status());
      return false;
    }
  } else {
    Serial.println("WLAN already conntected");
    Serial.print("MAC:");
    macAdress = WiFi.macAddress();
    Serial.println(macAdress);
  }

  return true;
}

/**
 * Sends the controller in Deep-Sleep-Mode. 
 * Duration can be configured via const period
 */ 
void deepSleep() {
  Serial.println("Go to Sleep");
  digitalWrite(5, LOW);
  Serial.flush();
  //ESP.deepSleep(10e6);
  //esp_sleep_enable_timer_wakeup(sleepTimeInSeconds * 1e6);
  //esp_deep_sleep_start();
  while(millis() < time_now + period){
        //do nothing until the next 60s ran up
        delay(100);
    }
  digitalWrite(5, HIGH);
  Serial.println("woke up");
}

/**
 * Saves the given entry, so it is aviavable after longer WiFi-unaviavability
 * Just saves the last 1000 entrys, older entrys will get lost
 */ 
void persiste(String jsonString) {
  Serial.print("Persisting:");
  Serial.println(jsonString);
  if (myqueue.size() > 750) {
    String s = myqueue.front(); //just save the last 750 entrys cause of memory
    Serial.println("Queue full, removing " + s);
  }
  myqueue.push(jsonString);
}

/**
 * Configures the time-lib to the western european time
 */
void synchronizeTime() {
  Serial.println("Synchronizing Time via: " + String(ntpServer));
  tryConnectWLAN();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //configTzTime(TZ_INFO, ntpServer); // ESP32 Systemzeit mit NTP Synchronisieren);
  getTimestamp();
}

/**
 * Reads the actual sensor data from the given sensor and builds a JSON-String
 * @return the JSON-Data in the format ready to send to IWI-Server
 */ 
String readSensorDataToJSON(CCS811 sensor) {
  Serial.println("Reading results from sensor");
  DynamicJsonDocument  doc(1000);
  mySensor.readAlgorithmResults();
  delay(50);
  doc["timestamp"] = getTimestamp();
  doc["macAdress"] = macAdress;
  doc["temperature"] = mySensor.getTemperature();
  doc["co2"] = mySensor.getCO2();
  doc["tvoc"] = mySensor.getTVOC();
  doc["altitude"] = mySensor.getBaseline();
  doc["errorstatus"] = mySensor.checkForStatusError();
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println("Sensor-Results: " + jsonString);
  return jsonString;
}

/*
 * Trys to send all data, which are written on the controller
 * @return true if all data has succesfully been sent 
 *   and no more data is aviavable to send
 */
bool sendHistoricalData() {
  Serial.println("Try sending historical Data");
  while (!myqueue.empty()) {
    String actToSend = myqueue.front();
    if (!sendActData(actToSend)) { //if sending data not succesfull -> Connection-Probem, save value and continue in loop
      myqueue.push(actToSend); //saving not sent string
      Serial.println("Not all historial Data could have been sent, there are " + String(myqueue.size()) + " values left");
      return false;
    }
  }
  Serial.println("All historical Data has been sent");
  return true;
}

/*
 * Trys to send the JSON-Data to the IWI-Server
 * @return true if the date has been sent succesfully and 
 *         false if the date hasn't been sent and has to be stored
 * 
 */
//TODO: implement Sending
bool sendActData(String jsonString) {
  Serial.println("Try to send: " + jsonString);
  //TODO: send
  Serial.println("Sending not successfull");
  return false;
}

/**
 * Returns the actual local timestamp formatted like specified  yyyy-MM-dd hh:mm:ss
 */
String getTimestamp() {
  Serial.println("Getting actual time");
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, 15000)){
    Serial.println("Failed to obtain time");
  }
  return formatTimestamp(timeinfo);
}

/**
 * Builds and prints a string out of the given timeInfo (struct tm)
 * The Format will be yyyy-mm-dd hh:mm:ss
 */
String formatTimestamp(tm info) {
  String stamp = "";
  stamp += String(info.tm_year + 1900) + "-";
  stamp = addZeroIfNeeded(stamp, info.tm_mon + 1, 10);
  stamp += String(info.tm_mon + 1) + "-";
  stamp = addZeroIfNeeded(stamp, info.tm_mday, 10);
  stamp += String(info.tm_mday) + " ";
  stamp = addZeroIfNeeded(stamp, info.tm_hour, 10);
  stamp += String(info.tm_hour) + ":";
  stamp = addZeroIfNeeded(stamp, info.tm_min, 10);
  stamp += String(info.tm_min) + ":";
  stamp = addZeroIfNeeded(stamp, info.tm_sec, 10);
  stamp += String(info.tm_sec);
  Serial.println(stamp);
  return stamp;
}

/**
 * Compares if the actualDateValue is under the threashold (toCompareTo) and adds an 0 to the string, if dateValue is smaller
 */
String addZeroIfNeeded(String stamp, int dateValue, int toCompareTo) {
  if (dateValue < toCompareTo) {
    stamp += "0";
  }
  return stamp;
}
