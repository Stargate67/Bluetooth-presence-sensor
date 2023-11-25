/*
  Rui Santos
  Complete project details
   - Arduino IDE: https://RandomNerdTutorials.com/esp32-ota-over-the-air-arduino/
   - VS Code: https://RandomNerdTutorials.com/esp32-ota-over-the-air-vs-code/
  
  This sketch shows a Basic example from the AsyncElegantOTA library: ESP32_Async_Demo
  https://github.com/ayushsharma82/AsyncElegantOTA
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "credentials.h"
#include "ModbusIP_ESP8266.h"
#include "BLEDevice.h"


/************************************************/
/*              Section MODBUS                  */
/************************************************/

// Modbus Registers Offsets
const int TEST_HREG = 1;
#define LEN 10

//ModbusIP object
ModbusIP mb;
int i=0;
int Cptr=0;
int iDevFound =0;
int MdbDevFound =0;
int MdbPresence = 0;
int period = 1; //10s
unsigned long time_now = 0;
unsigned long time1_now = 0;
  
/************************************************/
/*              FIN Section MODBUS              */
/************************************************/


struct strDevices {
  String Name;
  String Mac;
  String IP;
};

//const char* ssid = "REPLACE_WITH_YOUR_SSID";
//const char* password = "REPLACE_WITH_YOUR_PASSWORD";
int Lampe = 33;
static BLEAddress *pServerAddress;
BLEScan* pBLEScan;
BLEClient*  pClient;
bool deviceFound = false;
bool Allume = false;

strDevices knownDevices[3];
 
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice Device){
    //Serial.print("BLE Advertised Device found: ");
    //Serial.println(Device.toString().c_str());
    pServerAddress = new BLEAddress(Device.getAddress());
    deviceFound = false;
    mb.Hreg(0, deviceFound); // update local register with offset 0 by Status
    int i;
    for (i = 0; i < (sizeof(knownDevices) / sizeof(knownDevices[0])); i++) {
      if (strcmp(pServerAddress->toString().c_str(), knownDevices[i].Mac.c_str()) == 0) {
        Serial.print("Device found: ");
        Serial.print(knownDevices[i].Name);
        Serial.print(" RSSI= ");
        Serial.println(Device.getRSSI());
        deviceFound = true;
        mb.Hreg(0, deviceFound); // update local register with offset 0 by Status
        Device.getScan()->stop();
        break;
        //delay(100);
      }
    }
  }
}; 


void Bluetooth() {
  Serial.println();
  Serial.println("BLE Scan restarted.....");
  BLEScanResults scanResults = pBLEScan->start(3);
  //BLEScanResults scanResults = pBLEScan->start(3, (*scanCompleteCB)(BLEScanResults), false);
  Serial.println(scanResults.getCount());
  pBLEScan->clearResults();

  if (deviceFound) {
    iDevFound = 5;
    Allume = true;
    mb.Hreg(1, Allume); // update local register with offset 1 by Presence
  } else {
    if (iDevFound <1 ) {
      Allume = false;
      mb.Hreg(1, Allume); // update local register with offset 1 by Presence
    } else {
      iDevFound--;
    }
  }
}

AsyncWebServer server(80);

void setup(void) {
  Serial.begin(115200);

  knownDevices[0].Name = "MI10S FY";
  knownDevices[0].Mac = "bc:6a:d1:b0:29:fc";
  knownDevices[0].IP = "";

  knownDevices[1].Name = "XIAOMI Sous balcon";
  knownDevices[1].Mac = "a4:c1:38:73:e7:47";
  knownDevices[1].IP = "";

  knownDevices[2].Name = "Spare";
  knownDevices[2].Mac = "";
  knownDevices[2].IP = "";
  
  knownDevices[3].Name = "Spare";
  knownDevices[3].Mac = "";
  knownDevices[3].IP = "";

  BLEDevice::init("ESP32_BLESensor");
  pClient  = BLEDevice::createClient();
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(60);  // less or equal setInterval value
  Serial.println("Done");

  // Connect to Wi-Fi
  IPAddress ip(192, 168, 0, 48);   
  IPAddress gateway(192, 168, 0, 254);   
  IPAddress subnet(255, 255, 255, 0);   
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");

  /************************************************/
  /*              Section MODBUS SETUP            */
  /************************************************/
  mb.server();

  //création des registres
  //***********************************************
  //mb.addHreg(TEST_HREG, 0xABCD, LEN);
  mb.addHreg(0, 10, 1);
  mb.addHreg(1, 20, 1);
  mb.addHreg(2, 30, 1);
  mb.addHreg(3, 40, 1);

  /************************************************/
  /*     FIN Section MODBUS SETUP                 */
  /************************************************/
}

void loop(void) {

  Bluetooth();

  /************************************************/
  /*           Section MODBUS Main loop           */
  /************************************************/
  Cptr++;
  if (Cptr>65535) Cptr=0;

  if(millis() >= time1_now + 100) { //Process MB client request each second
    time1_now = millis();

    //Voir doc API PDF dans la librairie "modbus-esp8266-master"
    mb.Hreg(2, Cptr); // update local register with offset 3 by counter

    //Call once inside loop() - all magic here
    mb.task();
  }
  //delay(3000);
  //mb.task();
}