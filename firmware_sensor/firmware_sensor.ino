#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_sleep.h> 

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID           "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define VIBRATION_THRESHOLD 1.2     
#define IDLE_TIME_LIMIT     60000   
#define WAKE_UP_INTERVAL    15      

Adafruit_MPU6050 mpu;
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

bool deviceConnected = false;
unsigned long lastMotionTime = 0; 

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Gateway (Client) Connected");
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Gateway Disconnected");
        BLEDevice::startAdvertising(); 
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting SmartGuard Sensor...");

    if (!mpu.begin()) {
        Serial.println("MPU6050 Chip tidak ditemukan! Cek kabel!");
        while (1) { delay(10); } 
    }
    
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("MPU6050 Ready!");

    BLEDevice::init("SMARTGUARD_SENSOR"); 
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
                        CHAR_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_NOTIFY
                        );

    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); 
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE Advertising started. Menunggu Raka...");
    
    lastMotionTime = millis();
}

void loop() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float totalAccel = sqrt(pow(a.acceleration.x, 2) + 
                            pow(a.acceleration.y, 2) + 
                            pow(a.acceleration.z, 2));
    
    float vibrationIndex = fabs(totalAccel - 9.8); 

    Serial.print("Vib Index: ");
    Serial.println(vibrationIndex);

    if (vibrationIndex > VIBRATION_THRESHOLD) {
        lastMotionTime = millis();
    }

    if (deviceConnected) {
        char txString[8]; 
        dtostrf(vibrationIndex, 1, 2, txString); 
        
        pCharacteristic->setValue(txString);
        pCharacteristic->notify(); 
    }

    if (millis() - lastMotionTime > IDLE_TIME_LIMIT) {
        Serial.println("Mesin Idle. Masuk Deep Sleep...");
        
        esp_sleep_enable_timer_wakeup(WAKE_UP_INTERVAL * 1000000ULL); 
        
        esp_deep_sleep_start();
    }

    delay(200); 
}