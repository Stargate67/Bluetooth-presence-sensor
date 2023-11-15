#include <Arduino.h>
#include "BLEDevice.h"

struct strDevices {
String Name;
String Mac;
String IP;
};

int Lampe = 33;
static BLEAddress *pServerAddress;
BLEScan* pBLEScan;
BLEClient*  pClient;
bool deviceFound = false;
bool Allume = false;

strDevices knownDevices[5];

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
      Serial.print("BLE Advertised Device found: ");
      Serial.println(Device.toString().c_str());
      pServerAddress = new BLEAddress(Device.getAddress()); 
      bool Master = false;
      int i;
      for (i = 0; i < (sizeof(knownDevices) / sizeof(knownDevices[0])); i++) {
        if (strcmp(pServerAddress->toString().c_str(), knownDevices[i].Mac.c_str()) == 0) {
          Serial.print("Device found: ");
          Serial.print(knownDevices[i].Name);
          Serial.print(" RSSI= ");
          Serial.println(Device.getRSSI());
          if (Device.getRSSI() > -85) {
            deviceFound = true;
          }
          else {
            deviceFound = false;
          }
          Device.getScan()->stop();
          delay(100);
        }
      }
    }
};

void setup() {

  knownDevices[0].Name = "MI10S FY";
  knownDevices[0].Mac = "bc:6a:d1:b0:29:fc";
  knownDevices[0].IP = "";

  knownDevices[1].Name = "XIAOMI Sous balcon";
  knownDevices[1].Mac = "a4:c1:38:73:e7:47";
  knownDevices[1].IP = "";

  Serial.begin(115200);
  pinMode(Lampe, OUTPUT);
  digitalWrite(Lampe, LOW);
  BLEDevice::init("");
  pClient  = BLEDevice::createClient();
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  Serial.println("Done");
}

void Bluetooth() {
  Serial.println();
  Serial.println("BLE Scan restarted.....");
  deviceFound = false;
  BLEScanResults scanResults = pBLEScan->start(5);
  if (deviceFound) {
    //Serial.println("Allumer la lampe");
    Allume = true;
    digitalWrite(Lampe, HIGH);
    delay(10000);
  }
  else{
    digitalWrite(Lampe, LOW);
    delay(1000);
  }
}

void loop() { 
  Bluetooth();
}
