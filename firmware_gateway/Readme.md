# IOT22-SmartGuard: GATEWAY NODE FIRMWARE DOCUMENTATION

## 1. Project Overview
Gateway Node berfungsi sebagai unit pemroses pusat (Central Processing Unit) dalam arsitektur sistem SmartGuard. Firmware ini dirancang menggunakan FreeRTOS untuk menangani dua proses krusial secara paralel: akuisisi data nirkabel melalui Bluetooth Low Energy (BLE) dan eksekusi logika keselamatan (Safety Logic) serta pelaporan data ke Cloud via WiFi.

## 2. Hardware Specification
*   Microcontroller: ESP32-WROOM-32
*   Operating Voltage: 5V (via USB/VIN) for Peripherals, 3.3V for Logic.
*   Actuators:
    *   Relay Module (Active High/Low Configurable) connected to GPIO 26.
    *   Active Buzzer connected to GPIO 27.
*   Connectivity: WiFi 802.11 b/g/n & BLE 4.2.

## 3. Code Architecture & Line-by-Line Analysis

Bagian ini menjelaskan struktur logika program secara mendetail.

### A. Libraries and Definitions
```cpp
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "..."
```
*   **Analysis:** Mendefinisikan kredensial yang diperlukan agar perangkat dapat dikenali oleh server Blynk. `BLYNK_PRINT Serial` mengaktifkan debug log Blynk ke Serial Monitor.

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <BLEDevice.h>
```
*   **Analysis:**
    *   `WiFi.h`: Mengelola stack TCP/IP untuk koneksi internet.
    *   `HTTPClient.h`: Digunakan untuk mengirim HTTP POST Request ke server Flask (Vercel).
    *   `BLEDevice.h`: Pustaka inti untuk mengelola radio Bluetooth pada ESP32.

### B. Hardware Configuration Macros
```cpp
const bool RELAY_ACTIVE_LOW = true; 
#define MESIN_ON  (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define MESIN_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)
```
*   **Analysis:** Abstraksi logika perangkat keras. Variabel `RELAY_ACTIVE_LOW` memungkinkan pengembang mengubah logika pemicu relay (High Trigger atau Low Trigger) hanya dengan mengubah satu baris kode, tanpa perlu menelusuri seluruh logika program. Macro `MESIN_ON/OFF` menerjemahkan logika manusia ke sinyal digital.

### C. RTOS Resource Allocation
```cpp
QueueHandle_t sensorQueue;
SemaphoreHandle_t wifiMutex;
```
*   **Analysis (Modul 1-5):**
    *   `sensorQueue`: Pipa komunikasi memori (FIFO Buffer) untuk mengirim data struct dari Core 0 (BLE Task) ke Core 1 (Logic Task) secara aman (thread-safe).
    *   `wifiMutex`: Mekanisme penguncian (Mutual Exclusion). Karena library WiFi ESP32 tidak sepenuhnya *thread-safe*, mutex ini mencegah terjadinya *race condition* atau *crash* saat dua task mencoba mengakses radio WiFi secara bersamaan.

### D. BLE Callback Logic (Interrupt Context)
```cpp
static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    std::string value((char*)pData, length);
    // Parsing logic...
    xQueueSend(sensorQueue, &paket, 0);
}
```
*   **Analysis:** Fungsi ini dipanggil secara asinkron oleh stack Bluetooth saat data diterima.
    *   `sscanf`: Memecah string format CSV ("Getaran,Suhu") menjadi variabel float terpisah.
    *   `xQueueSend(..., 0)`: Mengirim data ke antrian dengan waktu tunggu (timeout) 0. Ini krusial karena callback berjalan dalam konteks yang mirip *interrupt service routine* (ISR), sehingga tidak boleh ada proses *blocking* atau *delay*.

### E. RTOS Task 1: TaskBLEManager (Core 0)
```cpp
void TaskBLEManager(void *pvParameters) {
  // ... BLE Init ...
  for(;;) {
    if (!bleConnected) {
      BLEScanResults foundDevices = pBLEScan->start(3, false);
      // ... Connection Logic ...
    }
    vTaskDelay(pdMS_TO_TICKS(5000)); 
  }
}
```
*   **Analysis:**
    *   Task ini berjalan di Core 0 (Radio Core).
    *   `for(;;)`: Infinite loop standar RTOS.
    *   `pBLEScan->start(3, false)`: Proses pemindaian bersifat *blocking* selama 3 detik. Ini mencari perangkat yang mengiklankan UUID Layanan (`serviceUUID`) yang spesifik.
    *   `vTaskDelay`: Memberikan waktu istirahat pada CPU agar tidak terjadi *watchdog timer reset* dan memberikan kesempatan task lain (background tasks) untuk berjalan.

### F. RTOS Task 2: TaskAppLogic (Core 1)
```cpp
void TaskAppLogic(void *pvParameters) {
  for(;;) {
    if (xQueueReceive(sensorQueue, &data, portMAX_DELAY) == pdPASS) {
        // ... Logic ...
    }
  }
}
```
*   **Analysis:**
    *   `xQueueReceive(..., portMAX_DELAY)`: Task ini akan memasuki mode **Blocked** (Tidur/Idle) jika antrian kosong. Ini sangat efisien karena tidak memakan siklus CPU saat tidak ada data masuk. Begitu data masuk, task langsung bangun (Event Driven).

#### Safety Logic Block (Local Intelligence)
```cpp
if (vib > DANGER_LIMIT) {
   digitalWrite(PIN_RELAY, MESIN_OFF);
   // ...
}
```
*   **Analysis:** Logika ini dijalankan *segera* setelah data diterima dari Queue. Prioritas utama adalah mematikan mesin secara lokal. Ini memastikan latensi minimal (<100ms) karena tidak menunggu proses koneksi internet.

#### Cloud Reporting Block
```cpp
if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE) {
   Blynk.virtualWrite(...);
   http.POST(payload);
   xSemaphoreGive(wifiMutex);
}
```
*   **Analysis:**
    *   `xSemaphoreTake`: Meminta "token" akses WiFi. Jika token sedang dipakai task lain, task ini akan menunggu.
    *   `http.POST`: Operasi jaringan yang berat/lama. Dilakukan setelah aksi keselamatan lokal selesai.
    *   `xSemaphoreGive`: Mengembalikan token agar WiFi bisa digunakan proses lain.

### G. Setup & Loop
```cpp
void setup() {
  // ... Hardware Init ...
  xTaskCreatePinnedToCore(..., 0); // Task BLE -> Core 0
  xTaskCreatePinnedToCore(..., 1); // Task Logic -> Core 1
}

void loop() {
  vTaskDelete(NULL);
}
```
*   **Analysis:**
    *   `xTaskCreatePinnedToCore`: Menginstruksikan scheduler FreeRTOS untuk membagi beban kerja ke dua inti prosesor ESP32 secara spesifik.
    *   `vTaskDelete(NULL)`: Menghapus task `loop()` default Arduino. Dalam implementasi RTOS murni, `loop()` tidak diperlukan dan menghapusnya akan menghemat alokasi memori stack.

## 4. Conclusion
Firmware Gateway ini memenuhi persyaratan sistem waktu nyata (Real-Time System) dengan memisahkan akuisisi data dan logika kontrol ke dalam task berbeda. Mekanisme sinkronisasi data menggunakan Queue dan Mutex menjamin integritas data dan stabilitas sistem.
