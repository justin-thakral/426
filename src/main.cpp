#include <Arduino.h>
#include <BLEScan.h>
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
#include <RadioLib.h>



#define SERVICE_2_UUID "e7f51b90-77f1-46df-9bbb-3a6fb1e26c9f"

/****************Pin assignment for the Heltec V3 board******************/
static const int LORA_CS = 8; // Chip select pin
//static const int LORA_MOSI = 10;
//static const int LORA_MISO = 11;
static const int LORA_SCK = 9;
static const int LORA_NRST = 12; // Reset pin
static const int LORA_DIO1 = 14; // DIO1 switch
static const int LORA_BUSY = 13;
static const int BUTTON = 0;

/****************LoRa parameters (you need to fill these params)******************/
static const float FREQ = 950;
static const float BW = 15.6;
static const uint8_t SF = 7;
static const int8_t TX_PWR = 20;
static const uint8_t CR = 5;
static const uint8_t SYNC_WORD = (uint8_t)0x34;
static const uint16_t PREAMBLE = 8;

/****************Payload******************/
String tx_payload = "Lost Pet";
String rx_data;
volatile u8_t BLECheck = 0;                                               // Check How many BLE Pet scanners are around us
volatile u8_t LoRaCheck = 0;                                              // Check how many LoRa Pet scanners are around us
#define SCAN_TIME 10                                                      // Duration of scanning in seconds
std::__cxx11::string targetUUID = "a4f287d0-6583-403a-ab0d-f469b5037101"; // UUID Randomly Generated
bool a;
// Our two different tech declarations as globals (to be used in setup and loop)
BLEScan *scan;                                                       // BLE Scanner type for heltech boards
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_NRST, LORA_BUSY); // Given from Lab4 for LoRa

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void)
{
  a = true;
}
void buttonISR()
{
  a = true;
}

// CONDITION is a u8_t that defines what mode we will be broadcasting in.
u8_t CONDITION = 0;
volatile u8_t LORAMAX_THRESHHOLD = 0;
volatile u8_t LORAMED_THRESHHOLD = 0;

class cb : public BLEAdvertisedDeviceCallbacks
{ // callback class for advertisements to receive advertisements and send scan requests
  void onResult(BLEAdvertisedDevice adv)
  {
    if (adv.haveServiceUUID() && adv.getServiceUUID().toString() == targetUUID)
    {
      BLECheck++;
      LORAMAX_THRESHHOLD += 1;                     // Maximum possible devices that would recieve the LoRa signal
      LORAMED_THRESHHOLD = LORAMAX_THRESHHOLD / 2; // Average, useful for large houses to prevent toggling in central areas.
      Serial.println("[BLE] Found a correct service uuid");
      Serial.printf("[BLE] Device %s\n",adv.toString().c_str(),adv.getRSSI());
      
    }
  }
};

void setup()
{
  // Setup for the BLE functionality, the main functionality
  // since we dont want our pet to be lost!
  
  Serial.begin(115200);
  Serial.println("Starting 'Pet'...");
  BLEDevice::init("Pet");
  scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new cb());
  scan->setActiveScan(true);
  scan->setWindow(50);
  scan->setInterval(100);

  // Setup for the LoRa, the alternative functionality
  // For when our pet is lost out of the house!
  pinMode(BUTTON, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON), buttonISR, FALLING);
  int state = radio.begin(); // Given from Lab4 for LoRa
  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println("success!");
  }
  else
  {
    Serial.print("Failed: ");
    Serial.println(state);
    while (true)
      ;
  }

  state = radio.setBandwidth(BW);
  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.printf("BW intialization failed %d", state);
    while (true)
      ;
  }
  state = radio.setFrequency(FREQ);
  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.printf("Frequency intialization failed %d", state);
    while (true)
      ;
  }
  state = radio.setSpreadingFactor(SF);
  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.printf("SF intialization failed %d", state);
    while (true)
      ;
  }
  state = radio.setOutputPower(TX_PWR);
  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.printf("Output Power intialization failed %d", state);
    while (true)
      ;
  }

  state = radio.setCurrentLimit(140.0);
  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.printf("Current limit intialization failed %d", state);
    while (true)
      ;
  }
}

void loop()
{
  int state;
  // Check what condition we must be using, either BLE or LoRa
  switch (CONDITION)
  {
  case 0:
    Serial.println(" [BLE] Start a scan");
    scan->start(13, true);
    if (BLECheck <= 0)
    {
      Serial.println(" [BLE] Check <=0, Switch to Lora");
      CONDITION = 1;
    }
    else
    {
      Serial.println("[BLE] Check>0, No Lora Needed");
      Serial.printf("[BLE] Specifically, Check = %d\n", BLECheck);

      // Start advertising when BLECheck > 0
      BLEAdvertising *advertising = BLEDevice::getAdvertising();
      advertising->addServiceUUID(SERVICE_2_UUID); // Add your UUID here
      advertising->setScanResponse(true);
      advertising->setMinPreferred(0x06); // Android BLE scan workaround
      advertising->start();
      Serial.println("[BLE] Advertising started...");
      delay(1000);
      advertising->stop();
      BLECheck = 0;
      Serial.println("[BLE] check back to 0");
      // MUST Clear results so that on rescan we can see the same MACs
      // Did not work if didn't clear results fyi
      scan->clearResults();
    }
    break;
  case 1:

    Serial.println("[LoRa] Sending LoRa packets");
    scan->setActiveScan(false);
    state = radio.transmit(tx_payload);
    delay(1000);
    state = radio.readData(rx_data);

    if (state == RADIOLIB_ERR_NONE)
    {
      // Increment the counter for detected LoRa devices
      LoRaCheck++;
    }
    else if (state == RADIOLIB_ERR_CRC_MISMATCH)
    {
      Serial.println("[LoRa] CRC error!");
    }
    else
    {
      Serial.println("[LoRa] No devices detected.");
    }
    if (LoRaCheck > LORAMAX_THRESHHOLD)
    {
      Serial.println("How is this possible, LoRa Devices++");
      LORAMAX_THRESHHOLD++;
      LORAMED_THRESHHOLD = LORAMAX_THRESHHOLD / 2;
      CONDITION = -1;
    }
    else if (LoRaCheck >= LORAMED_THRESHHOLD)
    {
      LoRaCheck = 0;
      Serial.println("At least in the range of the house");
      CONDITION = -1;
    }

    break;
  default:
    CONDITION = 0;
    scan->setActiveScan(true);
  
  }
  delay(20000);
}
