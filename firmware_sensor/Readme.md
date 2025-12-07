# IOT22-SmartGuard: SENSOR NODE FIRMWARE DOCUMENTATION

## 1. Project Overview
Sensor Node adalah perangkat *Edge* yang dirancang untuk beroperasi dengan efisiensi daya tinggi. Firmware ini berfokus pada pembacaan sensor inersia (Accelerometer) menggunakan protokol komunikasi tingkat rendah (Low-Level I2C) untuk menghindari overhead dan instabilitas library standar. Fitur utama meliputi deteksi aktivitas mesin dan manajemen daya otomatis (Deep Sleep).

## 2. Hardware Specification
*   Microcontroller: ESP32-WROOM-32
*   Sensor: MPU6050 (6-Axis Gyroscope & Accelerometer).
*   Power Source: Li-Ion Battery / Powerbank (3.7V - 5V).
*   Communication: Bluetooth Low Energy (BLE) Server Mode.

## 3. Code Architecture & Line-by-Line Analysis

### A. Configuration Macros
```cpp
#define MPU_ADDR 0x68
#define THRESHOLD_IDLE 1.2
#define TIME_TO_SLEEP_MS 30000
```
*   **Analysis:** Konstanta `0x68` adalah alamat default I2C MPU6050. `THRESHOLD_IDLE` (1.2 m/s²) adalah ambang batas untuk menentukan apakah mesin sedang bekerja atau mati. `TIME_TO_SLEEP_MS` menentukan seberapa lama sensor menunggu dalam keadaan diam sebelum mematikan diri sendiri.

### B. Hardware Abstraction Layer (Manual I2C)
```cpp
void setupMPU_Manual() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // Register PWR_MGMT_1
  Wire.write(0);     // Set bit 0 (Wake Up)
  Wire.endTransmission(true);
}
```
*   **Analysis:** MPU6050 secara default berada dalam mode *Sleep* saat pertama kali dinyalakan. Fungsi ini menulis nilai 0 ke register manajemen daya (`0x6B`) untuk membangunkan chip sensor agar mulai mengambil sampel data.

### C. RTOS Task: TaskSensorLogic
Task ini menggantikan `void loop()` dan berisi seluruh logika operasional sensor.

#### 1. Data Acquisition (Raw Register Access)
```cpp
Wire.beginTransmission(MPU_ADDR);
Wire.write(0x3B); // Register awal data Accelerometer
Wire.endTransmission(false);
Wire.requestFrom(MPU_ADDR, 14, true);
```
*   **Analysis:** Teknik *Burst Read*. Daripada membaca satu per satu, kode ini meminta 14 byte data sekaligus mulai dari alamat `0x3B`. Ini jauh lebih efisien secara waktu bus I2C.

```cpp
int16_t AcX = Wire.read() << 8 | Wire.read();
```
*   **Analysis (Bitwise Operation):** Data sensor MPU6050 adalah 16-bit yang terpecah menjadi 2 byte (High Byte dan Low Byte). Kode ini menggabungkan High Byte (digeser 8 bit ke kiri) dengan Low Byte menggunakan operasi OR (`|`) untuk merekonstruksi nilai integer 16-bit yang utuh.

#### 2. Signal Processing (Physics Calculation)
```cpp
float ax = AcX / 16384.0 * 9.8;
float totalAccel = sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2));
float vibrationIndex = fabs(totalAccel - 9.8);
```
*   **Analysis:**
    *   `16384.0`: Faktor skala sensitivitas untuk range +/- 2g.
    *   `sqrt(...)`: Menghitung magnitude vektor resultan dari 3 sumbu (X, Y, Z). Ini menghasilkan nilai total guncangan tanpa mempedulikan orientasi sensor.
    *   `- 9.8`: *Gravity Compensation*. Menghilangkan efek gravitasi bumi agar saat sensor diam, nilainya mendekati 0, bukan 9.8 m/s².

#### 3. Data Transmission (BLE Notify)
```cpp
snprintf(txBuffer, sizeof(txBuffer), "%.2f,%.2f", vibrationIndex, temperature);
pCharacteristic->setValue(txBuffer);
pCharacteristic->notify();
```
*   **Analysis (Modul 6):**
    *   `snprintf`: Memformat data float menjadi string CSV (Comma Separated Values) agar ringan dikirim.
    *   `notify()`: Menggunakan mekanisme *Push Notification* BLE. Data langsung "ditembakkan" ke Gateway tanpa menunggu Gateway meminta (polling). Ini menghemat daya dan bandwidth.

#### 4. Power Management (Deep Sleep State Machine)
```cpp
if (vibrationIndex > THRESHOLD_IDLE) {
  lastMotionTime = millis();
}

if (millis() - lastMotionTime > TIME_TO_SLEEP_MS) {
  BLEDevice::deinit();
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_SEC * 1000000ULL);
  esp_deep_sleep_start();
}
```
*   **Analysis (Modul 8):**
    *   Logika *Software Watchdog*: Setiap kali ada getaran, timer `lastMotionTime` di-reset.
    *   `millis() - lastMotionTime`: Menghitung durasi keheningan (idle duration).
    *   `esp_deep_sleep_start()`: Perintah untuk mematikan CPU utama, Cache, dan Radio. Hanya RTC (Real Time Clock) yang menyala. Konsumsi daya turun drastis dari ~100mA ke ~10µA.
    *   Sistem akan bangun kembali (Reboot) setelah timer habis.

### D. Setup Implementation
```cpp
Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
Wire.setClock(100000);
```
*   **Analysis:** Memaksa inisialisasi I2C pada pin fisik yang benar dan menurunkan kecepatan *clock* ke 100kHz (Standard Mode). Ini dilakukan untuk meningkatkan stabilitas sinyal pada kabel jumper yang panjang atau kualitas breadboard yang kurang baik.

## 4. Conclusion
Firmware Sensor Node berhasil mengimplementasikan prinsip *Edge Computing* (pemrosesan sinyal vektor lokal) dan *Green IoT* (manajemen daya Deep Sleep). Penggunaan akses register manual terbukti meningkatkan stabilitas sistem dibandingkan penggunaan library pihak ketiga yang berat.
