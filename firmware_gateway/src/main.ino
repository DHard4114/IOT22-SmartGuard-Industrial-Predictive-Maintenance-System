/*
 * IOT22-SmartGuard: Gateway Node
 * Modules: FreeRTOS, BLE Client, WiFi, HTTP, Blynk, Relay
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <BLEDevice.h>

// --- USER CONFIGURATION (WAJIB DIISI) ---
#define BLYNK_TEMPLATE_ID   "ISI_TEMPLATE_ID_DISINI"
#define BLYNK_TEMPLATE_NAME "SmartGuard"
#define BLYNK_AUTH_TOKEN    "ISI_AUTH_TOKEN_DISINI"

#define WIFI_SSID           "ISI_NAMA_WIFI"
#define WIFI_PASS           "ISI_PASSWORD_WIFI"

// URL dari Vercel (Contoh: https://iot22-project.vercel.app/api/log)
#define API_URL             "ISI_URL_VERCEL_DISINI"

// --- HARDWARE PIN ---
#define RELAY_PIN   26  // Terhubung ke Motor/Mesin
#define BUZZER_PIN  27  // Terhubung ke Alarm

// --- SAFETY PARAMETERS ---
#define VIB_DANGER_LIMIT 5.5  // Batas getaran bahaya (m/s^2)

// --- GLOBALS & RTOS HANDLES ---
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

QueueHandle_t sensorQueue;     // Antrian data dari BLE ke Logic
SemaphoreHandle_t wifiMutex;   // Mutex untuk mengamankan WiFi
bool bleConnected = false;
bool systemLocked = false;     // Status Emergency Stop

// --- BLE CALLBACK ---
// Dipanggil otomatis saat Sensor Node mengirim notify()
static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    std::string value((char*)pData, length);
    float vibration = std::stof(value); // Convert String ke Float
    
    // Masukkan data ke Queue (Time to wait: 0 karena ini ISR context-like)
    xQueueSend(sensorQueue, &vibration, 0);
}

// --- TASK 1: BLE MANAGER (Core 0) ---
// Bertugas mencari dan menjaga koneksi ke Sensor Node
void TaskBLE(void *pvParameters) {
  BLEDevice::init("");
  while(1) {
    if (!bleConnected) {
      BLEScan* pBLEScan = BLEDevice::getScan();
      pBLEScan->setActiveScan(true);
      pBLEScan->setInterval(100);
      pBLEScan->setWindow(99);
      
      BLEScanResults foundDevices = pBLEScan->start(3, false);
      
      for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice d = foundDevices.getDevice(i);
        
        // Cek apakah device yang ditemukan memiliki UUID yang sesuai
        if (d.haveServiceUUID() && d.isAdvertisingService(serviceUUID)) {
          BLEClient* pClient = BLEDevice::createClient();
          if (pClient->connect(&d)) {
             BLERemoteService* pS = pClient->getService(serviceUUID);
             if (pS) {
                BLERemoteCharacteristic* pC = pS->getCharacteristic(charUUID);
                if (pC && pC->canNotify()) {
                   pC->registerForNotify(notifyCallback);
                   bleConnected = true;
                   Serial.println("[BLE] Connected to Sensor Node");
                }
             }
          }
        }
      }
      pBLEScan->clearResults(); // Bersihkan memori scan
    }
    
    // Cek koneksi berkala
    // Jika putus, flag bleConnected akan dihandle di loop berikutnya
    vTaskDelay(5000 / portTICK_PERIOD_MS); 
  }
}

// --- TASK 2: SYSTEM LOGIC & CLOUD (Core 1) ---
// Bertugas mengambil keputusan (Actuator) dan lapor (Cloud)
void TaskLogic(void *pvParameters) {
  float currentVib;
  HTTPClient http;

  while(1) {
    // Tunggu sampai ada data di Queue (Blocking wait)
    if (xQueueReceive(sensorQueue, &currentVib, portMAX_DELAY)) {
      
      // 1. SAFETY LOGIC (Immediate Response)
      if (currentVib > VIB_DANGER_LIMIT) {
        digitalWrite(RELAY_PIN, HIGH); // Cut Power (Normally Open config)
        digitalWrite(BUZZER_PIN, HIGH);
        systemLocked = true;
        Serial.printf("[DANGER] Vibration High: %.2f! Machine Stopped.\n", currentVib);
      } else if (!systemLocked) {
        // Jika sistem tidak terkunci (aman), Relay Low (Nyala)
        digitalWrite(RELAY_PIN, LOW); 
        digitalWrite(BUZZER_PIN, LOW);
      }

      // 2. CLOUD REPORTING (Protected by Mutex)
      if (xSemaphoreTake(wifiMutex, portMAX_DELAY)) {
        
        // Kirim ke Blynk
        Blynk.virtualWrite(V0, currentVib); // Gauge Widget
        
        if (systemLocked) {
           Blynk.logEvent("anomaly_alert", "BAHAYA: Getaran Mesin Tinggi!");
           Blynk.virtualWrite(V1, 1); // LED Widget Merah ON
           
           // Kirim Log ke Flask (Hanya saat bahaya agar tidak spam)
           if (WiFi.status() == WL_CONNECTED) {
             http.begin(API_URL);
             http.addHeader("Content-Type", "application/json");
             String jsonPayload = "{\"vibration\":" + String(currentVib) + ", \"status\":\"DANGER\"}";
             int httpCode = http.POST(jsonPayload);
             http.end();
             Serial.printf("[CLOUD] Log sent via HTTP: %d\n", httpCode);
           }
        } else {
           Blynk.virtualWrite(V1, 0); // LED Widget OFF
        }
        
        Blynk.run(); // Proses Blynk housekeeping
        xSemaphoreGive(wifiMutex);
      }
    }
  }
}

// --- BLYNK RESET HANDLER (V2 Button) ---
BLYNK_WRITE(V2) {
  int val = param.asInt();
  if (val == 1) {
    systemLocked = false;
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("[MANUAL] System Reset via Blynk App");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Init Hardware
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  // Default state: Mesin Jalan (Relay LOW)
  digitalWrite(RELAY_PIN, LOW); 
  digitalWrite(BUZZER_PIN, LOW);

  // Init RTOS Objects
  sensorQueue = xQueueCreate(20, sizeof(float)); // Buffer 20 data
  wifiMutex = xSemaphoreCreateMutex();

  // Connect WiFi & Blynk
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Create Tasks
  // TaskBLE di Core 0 (Radio Core), TaskLogic di Core 1 (App Core)
  xTaskCreatePinnedToCore(TaskBLE, "BLE_Manager", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskLogic, "System_Logic", 8192, NULL, 2, NULL, 1);
}

void loop() {
  // Loop utama tidak digunakan karena semua logika ada di RTOS Tasks.
  // Menghapus task loop() untuk menghemat memori stack.
  vTaskDelete(NULL);
}
