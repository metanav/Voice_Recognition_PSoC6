#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Fujitsu.h>

#define SERVICE_UUID   "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CMD_CHAR_UUID "d4707b26-85a3-4465-ade4-afb370cf58c7"
#define SET_CHAR_UUID "d4707b26-85a3-4465-ade4-afb370cf58c8"

void ac_on();
void ac_off();
void temp_up();
void temp_down();

String ac_status;
uint8_t temp;

BLEServer* pServer = NULL;
BLECharacteristic* pCharCmd = NULL;
BLECharacteristic* pCharSet = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

const uint16_t kIrLed = 12;
IRFujitsuAC ac(kIrLed);

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Disconnected");
    }
};


class CharCallbacks: public BLECharacteristicCallbacks {

    void onWrite(BLECharacteristic *pCharacteristic) {
      Serial.println("onWrite called");
      std::string value = pCharacteristic->getValue();

      if (value.length() > 0) {
        Serial.println("Command");
        if (value == "AC_ON") {
          ac_on();
        } else if (value == "AC_OFF") {
          ac_off();

        } else if (value == "TEMP_UP") {
          temp_up();

        } else if (value == "TEMP_DOWN") {
          temp_down();
        }
      }
    }
};

void ac_on() {
  ac.begin();
  delay(200);
  ac.setModel(ARRAH2E);
  ac.setSwing(kFujitsuAcSwingOff);
  ac.setMode(kFujitsuAcModeHeat);
  ac.setFanSpeed(kFujitsuAcFanLow);
  ac.setTemp(20);
  ac.setCmd(kFujitsuAcCmdTurnOn);
  ac.send();
  ac_status = "AC_ON";
}

void ac_off() {
  ac.begin();
  delay(200);
  ac.setModel(ARRAH2E);
  ac.setSwing(kFujitsuAcSwingOff);
  ac.setMode(kFujitsuAcModeHeat);
  ac.setFanSpeed(kFujitsuAcFanLow);
  ac.setTemp(20);
  ac.setCmd(kFujitsuAcCmdTurnOff);
  ac.send();
  ac_status = "AC_OFF";
}

void temp_up() {
  ac.begin();
  delay(200);
  ac.setModel(ARRAH2E);
  ac.setSwing(kFujitsuAcSwingOff);
  ac.setMode(kFujitsuAcModeHeat);
  ac.setFanSpeed(kFujitsuAcFanLow);
  temp = ac.getTemp();
  ac.setTemp(temp + 1);
  ac.setCmd(kFujitsuAcCmdStayOn);
  ac.send();

} 

void temp_down() {
  ac.begin();
  delay(200);
  ac.setModel(ARRAH2E);
  ac.setSwing(kFujitsuAcSwingOff);
  ac.setMode(kFujitsuAcModeHeat);
  ac.setFanSpeed(kFujitsuAcFanLow);
  temp = ac.getTemp();
  ac.setTemp(temp - 1);
  ac.setCmd(kFujitsuAcCmdStayOn);
  ac.send();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Peripheral");
  ac_status = "AC_OFF";
  
  BLEDevice::init("REMOTE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharCmd = pService->createCharacteristic(
               CMD_CHAR_UUID,
               BLECharacteristic::PROPERTY_READ |
               BLECharacteristic::PROPERTY_WRITE |
               BLECharacteristic::PROPERTY_WRITE_NR
             );
  pCharCmd->setCallbacks(new CharCallbacks());
  pCharCmd->addDescriptor( new BLEDescriptor((uint16_t)0x2901));

  pCharSet = pService->createCharacteristic(
               SET_CHAR_UUID,
               //                      BLECharacteristic::PROPERTY_READ   |
               //                      BLECharacteristic::PROPERTY_WRITE  |
               BLECharacteristic::PROPERTY_NOTIFY |
               BLECharacteristic::PROPERTY_INDICATE
             );
  pCharSet->addDescriptor( new BLEDescriptor((uint16_t)0x2902));

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
}

void loop() {
  // notify changed value
  if (deviceConnected) {
    if (ac_status == "AC_ON") {
      pCharSet->setValue((uint8_t*) &temp, 4);
      pCharSet->notify();
      delay(5000);
    }
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}
