#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "esp32-hal-rgb-led.h"
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
constexpr uint8_t WIFI_LED_PIN = 8;
BLEScan* pBLEScan;
BLEClient*  pClient;
bool deviceFound = false;
bool Allume = false;
bool wifiLedOn = false;

strDevices knownDevices[4];
 
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
      String address = Device.getAddress().toString().c_str();

      Serial.print("BLE | Nom=");
      Serial.print(Device.haveName() ? Device.getName().c_str() : "(sans nom)");
      Serial.print(" | RSSI=");
      Serial.print(Device.getRSSI());
      Serial.print(" dBm | ID=");
      Serial.print(address);
      Serial.print(" | UUID=");
      if (Device.haveServiceUUID()) {
        Serial.println(Device.getServiceUUID().toString().c_str());
      } else {
        Serial.println("(non annonce)");
      }

      for (size_t i = 0; i < (sizeof(knownDevices) / sizeof(knownDevices[0])); i++) {
        if (strcmp(address.c_str(), knownDevices[i].Mac.c_str()) == 0) {
          Serial.print("Device found: ");
          Serial.print(knownDevices[i].Name);
          Serial.print(" RSSI=");
          Serial.println(Device.getRSSI());
          deviceFound = true;
          mb.Hreg(0, deviceFound); // update local register with offset 0 by Status
          break;
        }
      }
    }
}; 


void Bluetooth() {
  Serial.println();
  Serial.println("BLE Scan restarted.....");
  deviceFound = false;
  mb.Hreg(0, deviceFound);
  BLEScanResults scanResults = pBLEScan->start(3);
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

void updateWifiLed() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifiLedOn) {
    neopixelWrite(WIFI_LED_PIN, 0, 0, connected ? 32 : 0);
    wifiLedOn = connected;
  }
}

void setup(void) {
  Serial.begin(115200);
  neopixelWrite(WIFI_LED_PIN, 0, 0, 0);

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
  pBLEScan->setWindow(99);  // less or equal setInterval value
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
  updateWifiLed();
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });

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
  updateWifiLed();

  /************************************************/
  /*           Section MODBUS Main loop           */
  /************************************************/
  Cptr++;
  if (Cptr>65535) Cptr=0;

  if(millis() >= time1_now + 50) { //Process MB client request each second
    time1_now = millis();

    //Voir doc API PDF dans la librairie "modbus-esp8266-master"
    mb.Hreg(2, Cptr); // update local register with offset 3 by counter

    //Call once inside loop() - all magic here
    mb.task();
  }
}