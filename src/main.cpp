#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include <Servo.h>

#include <NTPClient.h>
#include <time.h>

#include <WiFi.h>
#include <FirebaseESP32.h>

FirebaseData firebaseData;
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t value = 0;
const int LED_BLUE = 2;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define FIREBASE_HOST "feeda-dog.firebaseio.com" 
#define FIREBASE_AUTH "uaeSZ45QbuMhrdyf3tul395fshDPP94zEf6kjulO"
#define WIFI_SSID "Beralda"
#define WIFI_PASSWORD "mbx-1949"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

Servo motor;  // create servo object to control a servo
static const int servoPin = 13;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {

    void startaMotor(){
      int i = 0;
      while(i < 4) {
        for(int posDegree = 0; posDegree < 180; posDegree++) {
          Serial.println(posDegree);
          motor.write(posDegree);
          delay(10);
        }

        for(int posDegree = 180; posDegree >= 0; posDegree--) {
          Serial.println(posDegree);
          motor.write(posDegree);
          delay(10);
        }
        i++;
      }

      // reseta posição do motor
      delay(100);
      motor.write(180);            
      delay(100);
    }

    void writeFirebase(String timeStamp) {
      FirebaseJson json;
      String path = "/alimentou";

      Serial.println("------------------------------------");
      Serial.println("Push JSON test...");

      json.clear();
      json.addString("date", timeStamp);
      json.addDouble("duration", 8.0); // secconds
      json.addDouble("calories", 220.7); // kcal

      if (Firebase.pushJSON(firebaseData, path, json))
      {
        Serial.println("PASSED");
        Serial.println("PATH: " + firebaseData.dataPath());
        Serial.print("PUSH NAME: ");
        Serial.println(firebaseData.pushName());
        Serial.println("ETag: " + firebaseData.ETag());
        Serial.println("------------------------------------");
        Serial.println();
      }
      else
      {
        Serial.println("FAILED");
        Serial.println("REASON: " + firebaseData.errorReason());
        Serial.println("------------------------------------");
        Serial.println();
      }  
    }

    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

          std::string liga = "liga";
          if(liga.compare(rxValue)){
            timeClient.forceUpdate();

            startaMotor();

            String timeStamp = timeClient.getFormattedDate();
            writeFirebase(timeStamp);
          }
        }

        Serial.println();
        Serial.println("*********");
      }
};

void wifiConfig() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  motor.attach(servoPin);  // attaches the servo on pin 9 to the servo object

  wifiConfig();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  //Set database read timeout to 1 minute (max 15 minutes)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");
 
  // Initialize a NTPClient to get time
  timeClient.begin();

  // Create the BLE Device
  BLEDevice::init("Feeda - Grupo 5");

  pinMode(LED_BLUE, OUTPUT);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ   |
    BLECharacteristic::PROPERTY_WRITE  |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE
  );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
    // notify changed value
    if (deviceConnected) {
        pCharacteristic->setValue(&value, 1);
        pCharacteristic->notify();
       // value++;
       delay(10); // bluetooth stack will go into congestion, if too many packets are sent
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