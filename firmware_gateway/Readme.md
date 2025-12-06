# 📡 Firmware Gateway – SmartGuard Predictive Maintenance System

Gateway ESP32 ini bertugas sebagai pusat komunikasi dalam sistem **SmartGuard Industrial Predictive Maintenance**, menerima data dari **Sensor Node via BLE**, memprosesnya dengan **FreeRTOS**, dan meneruskan data melalui **WiFi** ke server/cloud.

---

## 🔥 Fitur Utama Gateway
- **FreeRTOS Multitasking**
  - BLE Client Task
  - WiFi Sender Task
  - Actuator Control Task
- **BLE Client Mode**
  - Scan Sensor Node
  - Connect via BLE
  - Menerima data getaran + suhu (MPU6050)
- **WiFi Communication**
  - Mengirim data ke REST API / MQTT
- **Kontrol Aktuator**
  - LED / Buzzer aktif jika threshold getaran terlampaui
- **Modular & Scalable**
  - Mudah dikembangkan untuk final project

---

## 🗂️ Struktur Folder
firmware-gateway/
├── src/
│ └── main.cpp # Firmware utama (FreeRTOS + BLE Client + WiFi)
├── platformio.ini # Konfigurasi board dan library
└── README.md # Dokumentasi modul

---

## ⚙️ Konfigurasi PlatformIO
File `platformio.ini` di folder ini berisi konfigurasi:

- **Board:** ESP32 Dev Module  
- **Framework:** Arduino  
- **Libraries:**
  - ESP32 BLE Arduino
  - ArduinoJSON
  - WiFi
  - FreeRTOS (built-in)

---

## 🧠 Arsitektur FreeRTOS pada Gateway

### **1. BLE Client Task**
- Scan Sensor Node  
- Connect via BLE  
- Baca characteristic data sensor  
- Push data ke `dataQueue`

### **2. WiFi Sender Task**
- Ambil data dari `dataQueue`  
- Kirim ke API/MQTT  
- Menjaga koneksi tetap aktif

### **3. Actuator Task**
- Monitor nilai getaran  
- Jika > threshold → Nyalakan LED / buzzer  

---

## 🛠️ Alur Kerja Gateway
1. Boot ESP32  
2. Inisialisasi WiFi & BLE  
3. FreeRTOS membuat 3 task:  
   - BLE Client  
   - WiFi Upload  
   - Actuator Control  
4. Sensor Node mengirim data via BLE  
5. Gateway memproses dan mengirim ke server  
6. Sistem mendeteksi potensi kerusakan (predictive maintenance)

---

## 👤 Developer
**Muttaqien – Firmware Gateway Developer**  
Tugas:
- Implementasi FreeRTOS pada Gateway  
- Manajemen task  
- Logika BLE Client  
- Kontrol aktuator  

---

## 📄 Catatan Tambahan
- Pastikan SSID dan Password WiFi diubah di `main.cpp`
- UUID BLE harus sesuai dengan Sensor Node
- API endpoint bisa disesuaikan pada bagian HTTP/MQTT

---

## 📦 Status
✔️ Struktur folder lengkap  
✔️ PlatformIO siap digunakan  
✔️ Siap untuk implementasi firmware ESP32  
