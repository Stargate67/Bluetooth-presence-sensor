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
// Modbus server TCP
#include "ModbusServerTCPasync.h"
#include "BLEDevice.h"


/************************************************/
/*              Section MODBUS                  */
/************************************************/
// Modbus Registers Offsets
const int TEST_HREG = 1;
#define LEN 10

//ModbusIP object
// Create server
ModbusServerTCPasync MBserver;
int i=0;
int Cptr=0;
int iDevFound =0;
int MdbDevFound =0;
int MdbPresence = 0;
int period = 1; //10s
unsigned long time_now = 0;
unsigned long time1_now = 0;
uint16_t memo[32];     

ModbusMessage FC03(ModbusMessage request) {
  ModbusMessage response;      // The Modbus message we are going to give back
  uint16_t addr = 0;           // Start address
  uint16_t words = 0;          // # of words requested
  request.get(2, addr);        // read address from request
  request.get(4, words);       // read # of words from request

  // Address overflow?
  if ((addr + words) > 20) {
    // Yes - send respective error response
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS);
  }
  // Set up response
  response.add(request.getServerID(), request.getFunctionCode(), (uint8_t)(words * 2));
  // Request for FC 0x03?
  if (request.getFunctionCode() == READ_HOLD_REGISTER) {
    // Yes. Complete response
    for (uint8_t i = 0; i < words; ++i) {
      // send increasing data values
      response.add((uint16_t)(addr + i));
    }
  } else {
    // No, this is for FC 0x04. Response is random
    for (uint8_t i = 0; i < words; ++i) {
      // send increasing data values
      response.add((uint16_t)random(1, 65535));
    }
  }
  // Send response back
  return response;
}

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
    //mb.Hreg(0, deviceFound); // update local register with offset 0 by Status
    int i;
    for (i = 0; i < (sizeof(knownDevices) / sizeof(knownDevices[0])); i++) {
      if (strcmp(pServerAddress->toString().c_str(), knownDevices[i].Mac.c_str()) == 0) {
        Serial.print("Device found: ");
        Serial.print(knownDevices[i].Name);
        Serial.print(" RSSI= ");
        Serial.println(Device.getRSSI());
        deviceFound = true;
        //mb.Hreg(0, deviceFound); // update local register with offset 0 by Status
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
    //mb.Hreg(1, Allume); // update local register with offset 1 by Presence
  } else {
    if (iDevFound <1 ) {
      Allume = false;
      //mb.Hreg(1, Allume); // update local register with offset 1 by Presence
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

// Set up test memory
  for (uint16_t i = 0; i < 32; ++i) {
    memo[i] = (i * 2) << 8 | ((i * 2) + 1);
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");

  // Define and start RTU server
  MBserver.registerWorker(1, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=1
  MBserver.registerWorker(1, READ_INPUT_REGISTER, &FC03);     // FC=04 for serverID=1
  MBserver.registerWorker(2, READ_HOLD_REGISTER, &FC03);      // FC=03 for serverID=2
  MBserver.start(502, 1, 20000);
}

void loop(void) {

  Bluetooth();

  /************************************************/
  /*           Section MODBUS Main loop           */
  /************************************************/
  Cptr++;
  if (Cptr>65535) Cptr=0;

  static unsigned long lastMillis = 0;
  if (millis() - lastMillis > 10000) {
    lastMillis = millis();
    Serial.printf("free heap: %d\n", ESP.getFreeHeap());
  }

}