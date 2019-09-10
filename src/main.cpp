#include "SparkFunCCS811.h"
#include <Arduino.h>
#include "main.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ctime>
#include <queue>

#define CCS811_ADDR 0x5B //Default I2C Address
//#define CCS811_ADDR 0x5A //Alternate I2C Address

const char * networkName = "test";
const char * networkPswd = "testtest";
const int sleepTimeInSeconds = 15;
String macAdress = "";
CCS811 mySensor(CCS811_ADDR);
int period = 60000;
unsigned long time_now = 0;
std::queue<String> myqueue;
int trysForWifiConnection = 200;

void setup()
{
  Wire.begin();
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH); //LED on -> active
  Serial.begin(11500);
  Serial.print("Started");
  initSensor();
  tryConnectWLAN();
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
      Serial.println("Connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("timed OUT");
      Serial.print(WiFi.status());
      return false;
    }
  } else {
    Serial.println(WiFi.status());
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
    }
  digitalWrite(5, HIGH);
}

/**
 * Saves the given entry, so it is aviavable after longer WiFi-unaviavability
 * Just saves the last 1000 entrys, older entrys will get lost
 */ 
void persiste(String jsonString) {
  Serial.print("Persisting:");
  Serial.println(jsonString);
  if (myqueue.size() > 1000) {
    String s = myqueue.front(); //just save the last 1000 entrys cause of memory
    Serial.println("Queue full, removing " + s);
  }
  myqueue.push(jsonString);
}

/**
 * Overrides the timestamp
 */
//TODO: not needed anymore?
void persisteTimestamp(String timestamp) {
  //persistend_timestamp = timestamp;
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
  doc["errorstatus"] = mySensor.begin();
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println(jsonString);
  return jsonString;
}

/*
 * Trys to send all data, which are written on the controller
 * @return true if all data has succesfully been sent 
 *   and no more data is aviavable to send
 */
bool sendHistoricalData() {
  while (!myqueue.empty()) {
    String actToSend = myqueue.front();
    if (!sendActData(actToSend)) { //if sending data not succesfull -> Connection-Probem, save value and continue in loop
      myqueue.push(actToSend); //saving not sended string
      return false;
    }
  }
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
  return false;
}

/**
 * Returns the actual timestamp from:
 * www.iwi.hs-karlsruhe.de/Intranetaccess/timestamp
 * or a predicted stamp (last timestamp + 1min)
 */
//TODO: using the timestamp of IWI-server for the first time, initialize the following time-lib:
//https://www.pjrc.com/teensy/td_libs_DateTime.html and then always just return the timestamp of the lib
String getTimestamp() {
  bool hasActualTimestamp = false;
  String timestamp = "";
  if (tryConnectWLAN()) { //get act. timestamp from server
  Serial.println("Try getting timestamp from IWI");
    WiFiClient client;
    client.setTimeout(10);
    if(!client.connect("www.iwi.hs-karlsruhe.de", 80)) {
      Serial.println("Failed to connect");
    } else {
      Serial.println("Connected to intranet");
    }
    //client.println(F("GET /Intranetaccess/timestamp"));
    client.println(F("GET /Intranetaccess/info/bulletinboard/INFB HTTP/1.0"));
    client.println(F("Host: www.iwi.hs-karlsruhe.de"));
    client.println(F("Connection: close"));

    if (client.println() == 0) {
      Serial.println(F("Failed to send request"));
    }
    //TODO: read/receive Timestamp and set flag true
    //hasActualTimestamp = true;
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    Serial.println(status);
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
    }
  }

  if (!hasActualTimestamp) { //read from cache
  Serial.println("Failed getting timestamp via IWI-Server");
  Serial.println("Reading timestamp from cache instead");
  //persistend_timestamp [18] += 1;
  //Serial.println(persistend_timestamp);
  //timestamp = persistend_timestamp;
    //read
    //increment predicted time
  }

  persisteTimestamp(timestamp);

  return timestamp;
}
