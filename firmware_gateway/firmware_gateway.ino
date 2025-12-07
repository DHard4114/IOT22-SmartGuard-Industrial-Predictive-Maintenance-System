/**
 * @file Gateway_Final.ino
 * @brief Central Controller untuk IOT22-SmartGuard.
 * 
 * @details
 * Firmware ini bertugas sebagai Gateway yang menerima data telemetri
 * dari Sensor Node via BLE, memproses logika keselamatan (Safety Logic)
 * secara lokal menggunakan RTOS, dan mengirimkan laporan ke Cloud.
 * 
 * @modules_implemented
 * [Modul 1-5] FreeRTOS: Task (Core 0/1), Queue (Struct Data), Mutex (WiFi Protection).
 * [Modul 6]   Bluetooth: BLE Client Scanner & Subscribe Notify.
 * [Modul 7]   Connectivity: WiFi Station & HTTP POST (JSON).
 * [Modul 9]   Interface: Blynk IoT Integration (Gauge, LED, Button).
 * 
 * @author Kelompok 22
 * @date December 2025
 */

// =========================================================
// 1. LIBRARIES & DEFINITIONS
// =========================================================
#define BLYNK_TEMPLATE_ID   "TMPL6MyLsx_GV"
#define BLYNK_TEMPLATE_NAME "SmartGuard"
#define BLYNK_AUTH_TOKEN "ohhmp9o6X33TSpo9zZzOSsHbp_H7KlUs"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <BLEDevice.h>

// =========================================================
// 2. USER CONFIGURATION (WAJIB DIISI)
// =========================================================
// --- WiFi Credentials ---
const char* ssid = "dapa"; 
const char* pass = "12345678";

// --- Backend Endpoint (Flask/Vercel) ---
const char* api_url = "https://finproiot22.vercel.app/log";

// --- Safety Thresholds ---
const float DANGER_LIMIT = 5.5; // Batas getaran (m/s^2)

// =========================================================
// 3. HARDWARE CONFIGURATION
// =========================================================
#define PIN_RELAY   26  // Pin Kontrol Relay
#define PIN_BUZZER  27  // Pin Kontrol Buzzer

// --- Konfigurasi Logika Relay (PENTING!) ---
// Ubah ke 'true' jika Relay nyala saat diberi LOW (Jumper di L)
// Ubah ke 'false' jika Relay nyala saat diberi HIGH (Jumper di H - Recommended)
const bool RELAY_ACTIVE_LOW = true; 

// Macro untuk mempermudah pembacaan logika
#define MESIN_ON  (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define MESIN_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

// =========================================================
// 4. RTOS RESOURCES & DATA STRUCTURES
// =========================================================

// Struktur Data untuk Queue (Menerima paket Getaran + Suhu)
typedef struct {
  float vibration;
  float temperature;
} SensorData;

// Handle untuk RTOS Objects
QueueHandle_t sensorQueue;      // [Modul 3] Queue
SemaphoreHandle_t wifiMutex;    // [Modul 4] Mutex

// Identitas BLE (Harus sama dengan Sensor Node)
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// Status Global
bool bleConnected = false;
bool systemLocked = false;      // True = Emergency Stop Aktif

// =========================================================
// 5. CALLBACKS (Interrupt Context)
// =========================================================

/**
 * @brief BLE Notification Callback
 * Dipanggil otomatis saat Sensor Node mengirim data baru.
 * Parsing data string "VIB,TEMP" menjadi struct.
 */
static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    // 1. Ambil Raw Data
    std::string value((char*)pData, length);
    
    // 2. Parsing CSV (Comma Separated Value)
    float vib = 0.0;
    float temp = 0.0;
    
    // Fungsi sscanf memecah string "5.2,42.5" menjadi dua float
    if (sscanf(value.c_str(), "%f,%f", &vib, &temp) == 2) {
        
        // 3. Masukkan ke Struct
        SensorData paket;
        paket.vibration = vib;
        paket.temperature = temp;

        // 4. Kirim ke Queue (Non-blocking / Timeout 0)
        xQueueSend(sensorQueue, &paket, 0);
    }
}

// =========================================================
// 6. RTOS TASKS
// =========================================================

/**
 * @task TaskBLEManager
 * @core 0 (Radio Core)
 * @brief Mengelola pencarian dan koneksi Bluetooth.
 * Dijalankan di Core 0 agar tidak mengganggu logika utama di Core 1.
 */
void TaskBLEManager(void *pvParameters) {
  BLEDevice::init("");
  
  for(;;) { // Infinite Loop pengganti void loop
    
    // Jika putus koneksi, lakukan scanning ulang
    if (!bleConnected) {
      BLEScan* pBLEScan = BLEDevice::getScan();
      pBLEScan->setActiveScan(true);
      pBLEScan->setInterval(100);
      pBLEScan->setWindow(99);
      
      // Scanning selama 3 detik (Blocking di task ini saja)
      BLEScanResults* foundDevices = pBLEScan->start(3, false);
      
      for (int i = 0; i < foundDevices->getCount(); i++) {
        BLEAdvertisedDevice d = foundDevices->getDevice(i);
        
        // Cek UUID Target
        if (d.haveServiceUUID() && d.isAdvertisingService(serviceUUID)) {
          
          BLEClient* pClient = BLEDevice::createClient();
          if (pClient->connect(&d)) {
             BLERemoteService* pS = pClient->getService(serviceUUID);
             if (pS) {
                BLERemoteCharacteristic* pC = pS->getCharacteristic(charUUID);
                if (pC && pC->canNotify()) {
                   // Subscribe ke notifikasi sensor
                   pC->registerForNotify(notifyCallback);
                   bleConnected = true;
                   Serial.println(F("[BLE] Connected to Sensor Node!"));
                }
             }
          }
        }
      }
      pBLEScan->clearResults(); // Bersihkan memori
    }
    
    // Delay RTOS agar Watchdog tidak marah
    vTaskDelay(pdMS_TO_TICKS(5000)); 
  }
}

/**
 * @task TaskAppLogic
 * @core 1 (Application Core)
 * @brief Mengelola Logika Keselamatan, Aktuator, dan Cloud.
 * Task ini "Tidur" sampai ada data masuk di Queue (Event Driven).
 */
void TaskAppLogic(void *pvParameters) {
  SensorData data; // Buffer untuk terima data
  HTTPClient http;

  for(;;) {
    // 1. Terima Data dari Queue (Blocking Wait)
    // Task ini tidak memakan CPU jika tidak ada data dari sensor
    if (xQueueReceive(sensorQueue, &data, portMAX_DELAY) == pdPASS) {
      
      float vib = data.vibration;
      float temp = data.temperature;

      // --- [LOGIC A] SAFETY SYSTEM (Local Intelligence) ---
      // Berjalan instan tanpa menunggu internet
      
      if (vib > DANGER_LIMIT) {
        // KONDISI BAHAYA DETEKSI
        if (!systemLocked) {
           digitalWrite(PIN_RELAY, MESIN_OFF);  // Matikan Mesin
           digitalWrite(PIN_BUZZER, HIGH);      // Nyalakan Alarm
           systemLocked = true;                 // Kunci Sistem
           Serial.println(F("[DANGER] EMERGENCY STOP!"));
        }
      } else {
        // KONDISI NORMAL
        if (!systemLocked) {
           digitalWrite(PIN_RELAY, MESIN_ON);   // Pastikan Mesin Jalan
           digitalWrite(PIN_BUZZER, LOW);       // Matikan Alarm
        }
      }

      // --- [LOGIC B] CLOUD REPORTING (Modul 7 & 9) ---
      // Menggunakan Mutex untuk melindungi akses WiFi agar thread-safe
      
      if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE) {
        
        // 1. Update Blynk Dashboard
        Blynk.virtualWrite(V0, vib);  // Gauge Getaran
        Blynk.virtualWrite(V3, temp); // Label Suhu
        
        if (systemLocked) {
           Blynk.virtualWrite(V1, 255); // LED Merah ON
           
           // 2. Kirim Log ke Flask (Hanya saat Bahaya)
           if (WiFi.status() == WL_CONNECTED) {
             http.begin(api_url);
             http.addHeader("Content-Type", "application/json");
             
             // Format JSON Payload
             String payload = "{\"vibration\":" + String(vib) + 
                              ",\"temperature\":" + String(temp) + 
                              ",\"status\":\"DANGER\"}";
             
             int httpCode = http.POST(payload);
             http.end(); // Tutup koneksi
             
             if (httpCode > 0) Serial.println(F("[CLOUD] Log sent"));
           }
        } else {
           Blynk.virtualWrite(V1, 0); // LED Merah OFF
        }
        
        // 3. Blynk Housekeeping
        Blynk.run();
        
        // Kembalikan Token Mutex
        xSemaphoreGive(wifiMutex);
      }
    }
  }
}

// =========================================================
// 7. BLYNK EVENT HANDLERS
// =========================================================

/** @brief Tombol Reset Manual di App Blynk (V2) */
BLYNK_WRITE(V2) {
  if (param.asInt() == 1) {
    systemLocked = false;               // Buka Kunci
    digitalWrite(PIN_RELAY, MESIN_ON);  // Nyalakan Mesin
    digitalWrite(PIN_BUZZER, LOW);      // Matikan Alarm
    Serial.println(F("[MANUAL] System Reset via Blynk"));
  }
}

// =========================================================
// 8. SETUP & LOOP
// =========================================================
void setup() {
  // Init Serial
  Serial.begin(115200);
  Serial.println(F("\nIOT22-SmartGuard Gateway"));

  // Init Hardware
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // Set Kondisi Awal Aman
  digitalWrite(PIN_RELAY, MESIN_ON); 
  digitalWrite(PIN_BUZZER, LOW);

  // Init RTOS Objects
  sensorQueue = xQueueCreate(5, sizeof(SensorData)); // Antrian 5 paket data (dari 10)
  wifiMutex = xSemaphoreCreateMutex();                // Mutex WiFi

  // Koneksi WiFi
  WiFi.begin(ssid, pass);
  Serial.print(F("Connecting to WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println(F("\n[WiFi] Connected!"));

  // Koneksi Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Create Tasks (Modul 1 RTOS)
  Serial.println(F("[RTOS] Starting Tasks..."));
  
  // Task 1: BLE Manager (Core 0) - Kurangi stack size
  xTaskCreatePinnedToCore(
    TaskBLEManager, 
    "BLE_Task", 
    2048, // DARI 4096 -> 2048 (Optimasi memory)
    NULL, 
    1, 
    NULL, 
    0
  );

  // Task 2: Application Logic (Core 1) - Kurangi stack size
  xTaskCreatePinnedToCore(
    TaskAppLogic, 
    "App_Logic", 
    4096, // DARI 8192 -> 4096 (Optimasi memory)
    NULL, 
    2,    // Prioritas lebih tinggi
    NULL, 
    1
  );
  
  Serial.println(F("[SYSTEM] Gateway Ready"));
}

void loop() {
  // Loop harus kosong dan dihapus dalam implementasi RTOS yang benar
  // Tugas idle default Arduino dimatikan untuk menghemat resource
  vTaskDelete(NULL);
}