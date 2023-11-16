#include <Arduino.h>
#include "BLEDevice.h"
#include "OTA.h"
#include "ModbusIP_ESP8266.h"

/************************************************/
/*              Section MODBUS                  */
/************************************************/

// Modbus Registers Offsets
const int TEST_HREG = 1;
#define LEN 10

//ModbusIP object
ModbusIP mb;
int i=0;
int MdbStatus =0;
int MdbPresence = 0;
int period = 10000; //10s
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
          //delay(100);
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

// Connect to Wi-Fi
  IPAddress ip(192, 168, 0, 48);   
  IPAddress gateway(192, 168, 0, 254);   
  IPAddress subnet(255, 255, 255, 0);   
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());
  
  setupOTA("BLE_Sensor", ssid, password);

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
    /**
   * Enable OTA update
   */
  ArduinoOTA.begin();
}

void Bluetooth() {
  Serial.println();
  Serial.println("BLE Scan restarted.....");
  deviceFound = false;
  BLEScanResults scanResults = pBLEScan->start(3);
  if (deviceFound) {
    //Serial.println("Allumer la lampe");
    Allume = true;
    digitalWrite(Lampe, HIGH);
    //delay(10000);
  } else {
    Allume = false;
    digitalWrite(Lampe, LOW);
    //delay(1000);
  }
}

void loop() { 
  #ifndef ESP32_RTOS
    ArduinoOTA.handle();
  #endif
  
  //Bluetooth();

  /************************************************/
  /*           Section MODBUS Main loop           */
  /************************************************/
    
  //if(millis() >= time_now + period) { //each 10 seconds
    time_now = millis();
    MdbStatus = Allume;
    MdbPresence = Allume;
 
    mb.Hreg(0, MdbStatus); // update local register with offset 0 by Status
    mb.Hreg(1, MdbPresence); // update local register with offset 1 by Presence
 
    i++;
    if (i>65535) i=0;
    //Voir doc API PDF dans la librairie "modbus-esp8266-master"
    mb.Hreg(2, i); // update local register with offset 2 by counter
  //}

  //if(millis() >= time1_now + 50){ //Process MB client request each second
    time1_now = millis();
    //Call once inside loop() - all magic here
    mb.task();
  //}
  /************************************************/
  /*         FIN Section MODBUS Main loop         */
  /************************************************/

}
