#include "SparkFunCCS811.h"
#include <Arduino.h>
#include "main.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ctime>

#define CCS811_ADDR 0x5B //Default I2C Address
//#define CCS811_ADDR 0x5A //Alternate I2C Address

const char * networkName = "WLAN Gastzugang";
const char * networkPswd = "12345678";
const int sleepTimeInSeconds = 15;
String macAdress = "";
CCS811 mySensor(CCS811_ADDR);
RTC_DATA_ATTR char persistend_timestamp [20] = "2019-01-01_12:00:00";
RTC_DATA_ATTR String persisted_values [10];

void setup()
{
  Wire.begin();
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  Serial.begin(11500);
  Serial.print("Started");
  initSensor();
  tryConnectWLAN();

}

void loop()
{

  mySensor.readAlgorithmResults();
  bool connected = tryConnectWLAN();
  String jsonString = readSensorDataToJSON(mySensor);
  //delay(1000);
  if (connected) {
    if(sendHistoricalData()) {
      if (!sendActData(jsonString)) {
        persiste(jsonString);
      } //only if sending all historical Data done
    }
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
    while (WiFi.status() != WL_CONNECTED && trys < 50) 
    {
      delay(1000);
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
 * Duration can be configured via const sleepTimeInSeconds
 */ 
void deepSleep() {
  Serial.println("Go to Sleep");
  digitalWrite(5, LOW);
  Serial.flush();
  WiFi.getSleep();
  //ESP.deepSleep(10e6);
  esp_sleep_enable_timer_wakeup(sleepTimeInSeconds * 1e6);
  esp_deep_sleep_start();
}

/**
 * Writes the given JSON to the cache-Area on the chip, 
 * so it's aviavable after deepSleep-phase
 */ 
void persiste(String jsonString) {
  Serial.print("Persisting:");
  Serial.println(jsonString);
  int first_free_index = 0;
  for(; first_free_index < (sizeof(persisted_values)/sizeof(*persisted_values)); first_free_index++) {
    if (persisted_values[first_free_index] == "") {
      break;
    }
  }
  persisted_values[first_free_index] = jsonString;
}

/**
 * Overrides the timestamp
 */
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
  for(int i = 0; i < (sizeof(persisted_values)/sizeof(*persisted_values)); i++) {
    Serial.println((int)(sizeof(persisted_values)/sizeof(*persisted_values)));
    Serial.println(i);
    if (persisted_values[i] == "") {
      Serial.println("Leer");
      
    } else {
      Serial.println("voll:");
      Serial.println(persisted_values[i]);
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
bool sendActData(String jsonString) {
  return false;
}

/**
 * Returns the actual timestamp from:
 * www.iwi.hs-karlsruhe.de/Intranetaccess/timestamp
 * or a predicted stamp (last timestamp + 1min)
 */
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
  persistend_timestamp [18] += 1;
  Serial.println(persistend_timestamp);
  timestamp = persistend_timestamp;
    //read
    //increment predicted time
  }

  persisteTimestamp(timestamp);

  return timestamp;
}
