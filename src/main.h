#ifndef _mainh_
#define _mainh_

void printTest();
void initSensor();
bool tryConnectWLAN();
void deepSleep();
String readSensorDataToJSON(CCS811 sensor);
void persiste(String);
bool sendHistoricalData();
bool sendActData(String);
String getTimestamp();
void synchronizeTime();
String formatTimestamp(tm);
String addZeroIfNeeded(String, int, int);


#endif