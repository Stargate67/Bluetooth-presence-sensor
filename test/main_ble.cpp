#include <Arduino.h>
#include "BLEDevice.h"

int Lampara = 33;
int Contador = 0;
static BLEAddress *pServerAddress;
BLEScan* pBLEScan;
BLEClient*  pClient;
bool deviceFound = false;
bool Encendida = false;
bool BotonOff = false;
String knownAddresses[] = { "bc:6a:d1:b0:29:f9", "bc:6a:d1:b0:29:fc"};
unsigned long entry;

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
      bool known = false;
      bool Master = false;
      for (int i = 0; i < (sizeof(knownAddresses) / sizeof(knownAddresses[0])); i++) {
        if (strcmp(pServerAddress->toString().c_str(), knownAddresses[i].c_str()) == 0) 
          known = true;
      }
      if (known) {
        Serial.print("Device found: ");
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
};

void setup() {
  Serial.begin(115200);
  pinMode(Lampara,OUTPUT);
  digitalWrite(Lampara,LOW);
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
    Serial.println("Encender Lamara");
    Encendida = true;
    digitalWrite(Lampara,HIGH);
    Contador = 0;
    delay(10000);
  }
  else{
    digitalWrite(Lampara,LOW);
    delay(1000);
  }
}

void loop() { 
  Bluetooth();
}
