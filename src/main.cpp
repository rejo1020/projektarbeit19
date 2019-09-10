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
#define TZ_INFO "WEST-1DWEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00" // Western European Time
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
const char* ntpServer = "de.pool.ntp.org";
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
        delay(100);
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
  if (myqueue.size() > 750) {
    String s = myqueue.front(); //just save the last 750 entrys cause of memory
    Serial.println("Queue full, removing " + s);
  }
  myqueue.push(jsonString);
}

/**
 * Configures the time-lib to the western european time
 */
//TODO: Device don't get time from server and offers just the initial-time
void synchronizeTime() {
  tryConnectWLAN();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  configTzTime(TZ_INFO, ntpServer); // ESP32 Systemzeit mit NTP Synchronisieren);
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
 * Returns the actual timestamp
 */
//TODO: formatting the timestamp like specified  yyyy-MM-dd hh:mm:ss
String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, 15000)){
    Serial.println("Failed to obtain time");
  }
  String timestamp = String(timeinfo.tm_year) + "-" + String(timeinfo.tm_mon) + "-" + timeinfo.tm_mday + " " + timeinfo.tm_hour + ":" + timeinfo.tm_min + ":" + timeinfo.tm_sec;
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println(timestamp);
  return timestamp;
}
