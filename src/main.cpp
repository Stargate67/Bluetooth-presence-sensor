#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "credentials.h"
#include "ModbusIP_ESP8266.h"
#include "BLEDevice.h"
#include <TFT_eSPI.h>
#include <ArduinoOTA.h>

TFT_eSPI tft = TFT_eSPI();

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
constexpr uint8_t WIFI_LED_PIN = 2;
BLEScan* pBLEScan;
BLEClient*  pClient;
bool deviceFound = false;
bool Allume = false;
bool wifiLedOn = false;

strDevices knownDevices[7];

struct strScannedDevice {
  String Name;
  String Mac;
  int RSSI;
  bool Known;
};

constexpr size_t MAX_DISPLAYED_DEVICES = 8;
strScannedDevice scannedDevices[MAX_DISPLAYED_DEVICES];
size_t scannedDeviceCount = 0;
int scannedTotalCount = 0;

void rememberScannedDevice(BLEAdvertisedDevice& device, const String& address) {
  String name = device.haveName() ? device.getName().c_str() : "(sans nom)";
  bool known = false;

  for (size_t i = 0; i < (sizeof(knownDevices) / sizeof(knownDevices[0])); i++) {
    if (address == knownDevices[i].Mac) {
      name = knownDevices[i].Name;
      known = true;
      break;
    }
  }

  size_t slot = scannedDeviceCount;
  for (size_t i = 0; i < scannedDeviceCount; i++) {
    if (scannedDevices[i].Mac == address) {
      slot = i;
      break;
    }
  }

  if (slot == scannedDeviceCount) {
    if (scannedDeviceCount < MAX_DISPLAYED_DEVICES) {
      scannedDeviceCount++;
    } else {
      slot = 0;
      for (size_t i = 1; i < scannedDeviceCount; i++) {
        if (scannedDevices[i].RSSI < scannedDevices[slot].RSSI) {
          slot = i;
        }
      }
      if (device.getRSSI() <= scannedDevices[slot].RSSI) {
        return;
      }
    }
  }

  scannedDevices[slot].Name = name;
  scannedDevices[slot].Mac = address;
  scannedDevices[slot].RSSI = device.getRSSI();
  scannedDevices[slot].Known = known;
}

void drawScanScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("BLE SCAN " + String(scannedTotalCount), 4, 2, 2);
  tft.drawFastHLine(4, 21, tft.width() - 8, TFT_DARKGREY);

  size_t visibleCount = scannedDeviceCount < 5 ? scannedDeviceCount : 5;
  int order[MAX_DISPLAYED_DEVICES];
  for (size_t i = 0; i < scannedDeviceCount; i++) {
    order[i] = i;
  }
  for (size_t i = 0; i < scannedDeviceCount; i++) {
    for (size_t j = i + 1; j < scannedDeviceCount; j++) {
      if (scannedDevices[order[j]].RSSI > scannedDevices[order[i]].RSSI) {
        int temp = order[i];
        order[i] = order[j];
        order[j] = temp;
      }
    }
  }

  if (visibleCount == 0) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Aucun appareil BLE", 4, 42, 2);
  }

  for (size_t i = 0; i < visibleCount; i++) {
    strScannedDevice& device = scannedDevices[order[i]];
    String name = device.Known ? "* " + device.Name : device.Name;
    while (name.length() > 0 && tft.textWidth(name, 2) > 166) {
      name.remove(name.length() - 1);
    }
    tft.setTextColor(device.Known ? TFT_GREENYELLOW : TFT_WHITE, TFT_BLACK);
    tft.drawString(name, 4, 25 + i * 17, 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(String(device.RSSI) + "dBm", 183, 25 + i * 17, 2);
  }

  tft.drawFastHLine(4, 112, tft.width() - 8, TFT_DARKGREY);
  tft.setTextColor(Allume ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.drawString(Allume ? "PRESENCE: OUI" : "PRESENCE: NON", 4, 116, 2);
}
 
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

      rememberScannedDevice(Device, address);

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
  scannedDeviceCount = 0;
  mb.Hreg(0, deviceFound);
  BLEScanResults scanResults = pBLEScan->start(2); // Scan de 2 secondes
  scannedTotalCount = scanResults.getCount();
  Serial.println(scannedTotalCount);
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

  drawScanScreen();
}

AsyncWebServer server(80);

void updateWifiLed() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifiLedOn) {
    digitalWrite(WIFI_LED_PIN, connected ? HIGH : LOW);
    wifiLedOn = connected;
  }
}

void setup(void) {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("BLE Sensor", 4, 8, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Initialisation...", 4, 48, 2);

  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);

  knownDevices[0].Name = "MI10S FY";
  knownDevices[0].Mac = "bc:6a:d1:b0:29:fc";
  knownDevices[0].IP = "";

  knownDevices[1].Name = "XIAOMI Sous balcon";
  knownDevices[1].Mac = "a4:c1:38:73:e7:47";
  knownDevices[1].IP = "";

  knownDevices[2].Name = "";
  knownDevices[2].Mac = "";
  knownDevices[2].IP = "";
  
  knownDevices[3].Name = "";
  knownDevices[3].Mac = "";
  knownDevices[3].IP = "";

  knownDevices[4].Name = "";
  knownDevices[4].Mac = "";
  knownDevices[4].IP = "";
  
  knownDevices[5].Name = "";
  knownDevices[5].Mac = "";
  knownDevices[5].IP = "";

  knownDevices[6].Name = "HONOR Band 5 Muriel";
  knownDevices[6].Mac = "18:d9:8f:54:22:e1";
  knownDevices[6].IP = "";

  BLEDevice::init("ESP32_BLESensor");
  pClient  = BLEDevice::createClient();
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(160);     // 100 ms
  pBLEScan->setWindow(80);        // 50 ms d'écoute
  Serial.println("Done");

  // Connect to Wi-Fi
  IPAddress ip(192, 168, 0, 48);   
  IPAddress gateway(192, 168, 0, 254);   
  IPAddress subnet(255, 255, 255, 0);   
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Keep the search indication visible even if Wi-Fi connects quickly.
  unsigned long wifiSearchStartedAt = millis();
  while (WiFi.status() != WL_CONNECTED || millis() - wifiSearchStartedAt < 2000) {
    wifiLedOn = !wifiLedOn;
    digitalWrite(WIFI_LED_PIN, wifiLedOn ? HIGH : LOW);
    delay(500);
    Serial.print(".");
  }
  updateWifiLed();
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ArduinoOTA.setHostname("ESP32_BLESensor");
  ArduinoOTA.onStart([]() {
    Serial.println("Mise a jour OTA demarree");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nMise a jour OTA terminee");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Erreur OTA: %u\n", error);
  });
  ArduinoOTA.begin();
  Serial.println("OTA pret");

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
  ArduinoOTA.handle();

  if (millis() - time_now >= 5000) { // Un scan toutes les 5 secondes
    time_now = millis();
    Bluetooth();
  }

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