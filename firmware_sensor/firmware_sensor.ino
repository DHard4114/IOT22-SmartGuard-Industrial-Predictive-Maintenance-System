/**
 * @file Node_Sensor_Final.ino
 * @brief Firmware Sensor Node untuk Proyek IOT22-SmartGuard.
 * 
 * @details
 * Kode ini mengimplementasikan pembacaan sensor inersia (MPU6050)
 * untuk mendeteksi anomali getaran dan suhu pada mesin industri.
 * Data dikirim secara nirkabel (BLE) dan node akan tidur otomatis
 * jika mesin mati untuk menghemat baterai.
 * 
 * @modules_implemented
 * [Modul 1-5] FreeRTOS: Penggunaan xTaskCreate, vTaskDelay, vTaskDelete.
 * [Modul 6]   Bluetooth: Implementasi BLE Server dan Notify.
 * [Modul 8]   Power Mgmt: Implementasi Deep Sleep & Wakeup Timer.
 * [Extra]     I2C Protocol: Akses register sensor manual (Raw Data).
 * 
 * @author Kelompok 22
 * @date December 2025
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Wire.h> // Library komunikasi I2C (Bawaan Arduino)

// =========================================================
// 1. KONFIGURASI SISTEM (User Config)
// =========================================================

// --- Identitas BLE (UUID) ---
// Note: UUID ini HARUS SAMA PERSIS dengan yang ada di Gateway
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID           "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- Konfigurasi Sensor (MPU6050) ---
#define MPU_ADDR            0x68  // Alamat I2C MPU6050 (Default)
#define I2C_SDA_PIN         21    // Pin Data
#define I2C_SCL_PIN         22    // Pin Clock

// --- Manajemen Daya (Deep Sleep) ---
// Jika getaran < 1.2 m/s^2 selama 30 detik, masuk mode tidur.
#define THRESHOLD_IDLE      1.2   
#define TIME_TO_SLEEP_MS    30000 
#define SLEEP_DURATION_SEC  10    

// =========================================================
// 2. VARIABEL GLOBAL (RTOS Shared Resources)
// =========================================================
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
unsigned long lastMotionTime = 0; // Timer deteksi diam

// =========================================================
// 3. FUNGSI HELPER (Hardware Abstraction)
// =========================================================

/**
 * @brief Membangunkan MPU6050 dari mode sleep default-nya.
 * Menulis ke register Power Management 1 (0x6B).
 */
void setupMPU_Manual() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  delay(100);        // Stabilisasi
}

/**
 * @brief Callback untuk event koneksi BLE
 * Mengelola status koneksi agar sistem tahu kapan harus kirim data.
 */
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println(F("[BLE] Gateway Connected!"));
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      // Modul 6: Restart advertising agar Gateway bisa connect ulang otomatis
      BLEDevice::startAdvertising();
      Serial.println(F("[BLE] Disconnected. Re-advertising..."));
    }
};

// =========================================================
// 4. RTOS TASK: LOGIKA UTAMA SENSOR
// =========================================================
// Task ini menggantikan void loop() untuk memenuhi standar RTOS.
void TaskSensorLogic(void *pvParameters) {
  
  // Buffer untuk menyimpan string data yang akan dikirim
  char txBuffer[20]; 

  // Inisialisasi MPU di dalam Task
  setupMPU_Manual();

  for(;;) { // Infinite Loop (Standar FreeRTOS)
    
    // --- LANGKAH 1: Request Data Raw dari MPU6050 ---
    // Kita baca mulai dari register 0x3B (ACCEL_XOUT_H)
    // Total 14 register berurutan: Accel(6) + Temp(2) + Gyro(6)
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);  
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 14, true);

    // --- LANGKAH 2: Baca & Parsing Data (Bit Shifting) ---
    // Baca Accelerometer (X, Y, Z)
    int16_t AcX = Wire.read() << 8 | Wire.read();
    int16_t AcY = Wire.read() << 8 | Wire.read();
    int16_t AcZ = Wire.read() << 8 | Wire.read();

    // Baca Temperature
    int16_t TmpRaw = Wire.read() << 8 | Wire.read();

    // Baca Gyroscope (Kita baca tapi tidak dipakai, agar buffer bersih)
    Wire.read(); Wire.read(); Wire.read(); Wire.read(); Wire.read(); Wire.read();

    // --- LANGKAH 3: Kalkulasi Fisika ---
    
    // A. Konversi Suhu (Rumus dari Datasheet MPU6050)
    // Temperature in degrees C = (TEMP_OUT Register Value as a signed quantity)/340 + 36.53
    float temperature = (TmpRaw / 340.0) + 36.53;

    // B. Konversi Akselerometer & Vektor Getaran
    // Skala default +/- 2g = 16384 LSB/g
    float ax = AcX / 16384.0 * 9.8; // Konversi ke m/s^2
    float ay = AcY / 16384.0 * 9.8;
    float az = AcZ / 16384.0 * 9.8;

    // Hitung Magnitude Vektor Total = sqrt(x^2 + y^2 + z^2)
    float totalAccel = sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
    
    // Filter Gravitasi: Kurangi 9.8 m/s^2 agar saat diam nilainya mendekati 0
    float vibrationIndex = fabs(totalAccel - 9.8);

    // --- LANGKAH 4: Kirim Data via BLE (Modul 6) ---
    // Format Protokol: "GETARAN,SUHU" (Contoh: "1.25,43.20")
    snprintf(txBuffer, sizeof(txBuffer), "%.2f,%.2f", vibrationIndex, temperature);

    if (deviceConnected) {
        pCharacteristic->setValue(txBuffer);
        pCharacteristic->notify(); // Push Notification ke Gateway
    }
    
    // Debugging ke Serial Monitor
    Serial.printf("[TASK] Vib: %.2f m/s^2 | Temp: %.2f C\n", vibrationIndex, temperature);

    // --- LANGKAH 5: Power Management (Modul 8) ---
    // Logika: Jika getaran > threshold, berarti mesin AKTIF. Reset timer idle.
    if (vibrationIndex > THRESHOLD_IDLE) {
      lastMotionTime = millis(); 
    }

    // Cek apakah sudah diam melebihi batas waktu?
    if (millis() - lastMotionTime > TIME_TO_SLEEP_MS) {
      Serial.println(F("[POWER] Machine Idle Timeout. Entering Deep Sleep..."));
      Serial.flush(); // Tunggu print selesai
      
      // Matikan BLE & I2C
      BLEDevice::deinit();
      
      // Konfigurasi Timer Wakeup (Bangun setelah X detik)
      esp_sleep_enable_timer_wakeup(SLEEP_DURATION_SEC * 1000000ULL);
      
      // Masuk Mode Deep Sleep (CPU Mati, Hemat Baterai)
      esp_deep_sleep_start();
    }

    // RTOS Delay: Memberi waktu istirahat pada CPU (200ms = 5Hz Sampling)
    vTaskDelay(pdMS_TO_TICKS(200)); 
  }
}

// =========================================================
// 5. SETUP UTAMA
// =========================================================
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  
  // Init I2C Manual (Paksa Pin 21 & 22 agar stabil)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000); // Set kecepatan 100kHz (Standard Mode)

  Serial.println(F("\n--- IOT22-SmartGuard Sensor Node (Final) ---"));

  // Init BLE (Modul 6)
  Serial.println(F("[INIT] Starting BLE Server..."));
  BLEDevice::init("SMARTGUARD_NODE"); // Nama Device yang muncul saat scan
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Buat Service BLE
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // Buat Characteristic (Wadah Data)
  pCharacteristic = pService->createCharacteristic(
                      CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pService->start();
  
  // Mulai Advertising (Menyebar Sinyal)
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); 
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println(F(">> SYSTEM READY. Waiting for Gateway connection..."));
  
  lastMotionTime = millis();

  // Create FreeRTOS Task (Modul 1-5)
  // Menjalankan logika sensor di Core 1 (Application Core)
  xTaskCreatePinnedToCore(
    TaskSensorLogic,   // Fungsi Task
    "SensorTask",      // Nama Task
    4096,              // Stack Size (4KB)
    NULL,              // Parameter
    1,                 // Prioritas (1 = Normal)
    NULL,              // Handle Task
    1                  // Core ID
  );
}

// =========================================================
// 6. MAIN LOOP
// =========================================================
void loop() {
  // Loop Dikosongkan & Dihapus (Sesuai kaidah RTOS Praktikum)
  // Ini menghemat memori karena task 'loop' default akan dimatikan.
  vTaskDelete(NULL); 
}