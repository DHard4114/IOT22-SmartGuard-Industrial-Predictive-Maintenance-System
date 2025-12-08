
# IOT22-SmartGuard: Industrial Predictive Maintenance System

**ANGGOTA KELOMPOK:**
1. Daffa Hardhan - 2306161763
2. Siti Amalia Nurfaidah - 2306161851
3. Raka Arrayan Muttaqien - 2306161800
4. Naufal Hadi Rasikhin - 2306231366

---

### TABLE OF CONTENTS

*   **CHAPTER 1: INTRODUCTION**
    *   1.1 Problem Statement
    *   1.2 Proposed Solution
    *   1.3 Acceptance Criteria
    *   1.4 Roles and Responsibilities
    *   1.5 Timeline and Milestones
*   **CHAPTER 2: IMPLEMENTATION**
    *   2.1 Hardware Design and Schematic
    *   2.2 Software Development
    *   2.3 Hardware and Software Integration
*   **CHAPTER 3: TESTING AND EVALUATION**
    *   3.1 Testing Methodology
    *   3.2 Result
    *   3.3 Evaluation
*   **CHAPTER 4: CONCLUSION**
*   **REFERENCES**

---

### CHAPTER 1: INTRODUCTION

#### 1.1 PROBLEM STATEMENT
Dalam industri manufaktur modern, kegagalan mesin yang tidak terprediksi (*downtime*) menyebabkan kerugian operasional yang signifikan. Metode pemantauan konvensional seringkali terkendala oleh pemasangan kabel sensor yang rumit pada mesin bergerak dan ketergantungan pada jaringan WiFi terpusat. Jika jaringan WiFi terputus, sistem pemantauan berbasis cloud akan gagal memberikan respons darurat, meningkatkan risiko kerusakan mesin permanen.

#### 1.2 PROPOSED SOLUTION
**IOT22-SmartGuard** mengusulkan sistem *Predictive Maintenance* berbasis IoT yang menggunakan arsitektur *Hybrid Network*. Sistem ini memisahkan peran antara **Sensor Node** (nirkabel, hemat daya) dan **Gateway Node** (pemrosesan cerdas). Sensor Node menggunakan protokol BLE (*Bluetooth Low Energy*) untuk mengirimkan data getaran presisi ke Gateway. Gateway menggunakan sistem operasi waktu nyata (**FreeRTOS**) untuk memproses data dan memicu aktuator keselamatan secara lokal tanpa latensi internet, sekaligus mencatat anomali ke Cloud Server untuk analisis jangka panjang.

#### 1.3 ACCEPTANCE CRITERIA
Proyek ini dinyatakan berhasil apabila memenuhi indikator berikut:
1.  **Real-Time Response:** Gateway dapat memutus daya mesin (Relay) dalam waktu < 1 detik setelah getaran melebihi ambang batas (5.5 m/s²).
2.  **Wireless Reliability:** Transmisi data dari Sensor Node ke Gateway via BLE stabil pada jarak minimal 5 meter.
3.  **Power Efficiency:** Sensor Node harus mampu memasuki mode *Deep Sleep* secara otomatis setelah 60 detik tidak ada aktivitas mesin.
4.  **Cloud Integration:** Log kejadian "DANGER" tersimpan di Flask Database dan notifikasi muncul di aplikasi Blynk.
5.  **Multitasking Stability:** Gateway tidak mengalami *blocking* atau *hang* pada fungsi Relay meskipun koneksi WiFi terputus.

#### 1.4 ROLES AND RESPONSIBILITIES
*   **Daffa Hardhan (Integrator & Tester):** Bertanggung jawab atas perakitan fisik (*wiring*), integrasi antara perangkat keras dan lunak, serta pelaksanaan pengujian sistem menyeluruh (*running test*) untuk memastikan reliabilitas alat.
*   **Siti Amalia Nurfaidah (Firmware Sensor):** Bertanggung jawab atas pengembangan kode Sensor Node, termasuk algoritma pembacaan sensor MPU6050, manajemen daya (*Deep Sleep*), dan server BLE.
*   **Raka Arrayan Muttaqien (Firmware Gateway):** Bertanggung jawab atas implementasi FreeRTOS pada Gateway, manajemen Task, logika BLE Client, dan kontrol aktuator.
*   **Naufal Hadi Rasikhin (Cloud & Full Stack):** Bertanggung jawab atas pengembangan Backend Flask, API deployment, visualisasi Dashboard Blynk, dan protokol komunikasi jaringan.

#### 1.5 TIMELINE AND MILESTONES
*   **Minggu 1:** Riset pustaka MPU6050 dan FreeRTOS. Pembuatan prototipe Sensor Node (Reading & BLE Server).
*   **Minggu 2 (Awal):** Implementasi Gateway dengan FreeRTOS (Task BLE Scan & Task Logic). Setup Backend Flask di Vercel.
*   **Minggu 2 (Akhir):** Integrasi sistem penuh. Pengujian latensi dan Deep Sleep. Penulisan laporan akhir.

---

### CHAPTER 2: IMPLEMENTATION

#### 2.1 HARDWARE DESIGN AND SCHEMATIC
Sistem terdiri dari dua subsistem utama:
1.  **Subsistem Sensor:** Menggunakan ESP32 yang ditenagai baterai Li-Ion. Sensor MPU6050 dihubungkan melalui protokol I2C (SDA: GPIO 21, SCL: GPIO 22).
2.  **Subsistem Gateway:** Menggunakan ESP32 dengan daya USB. Relay Module (GPIO 26) memutus jalur positif motor DC, dan Buzzer (GPIO 27) sebagai indikator audio.

#### 2.2 SOFTWARE DEVELOPMENT

**A. Algoritma Sensor Node (Deep Sleep & Vektor)**
Sensor membaca akselerasi pada 3 sumbu. Untuk mendeteksi getaran total tanpa terpengaruh orientasi sensor, digunakan rumus magnitude vektor dikurangi gravitasi bumi:
$$Vibration = | \sqrt{x^2 + y^2 + z^2} - 9.8 |$$
Jika `Vibration` di bawah threshold selama 60 detik, fungsi `esp_deep_sleep_start()` dipanggil dengan pemicu bangun `esp_sleep_enable_timer_wakeup()`.

**B. Arsitektur Firmware Gateway (FreeRTOS)**
Gateway menerapkan *Dual-Core Processing* menggunakan FreeRTOS:
*   **Core 0 (TaskBLE):** Didedikasikan untuk pemindaian radio BLE dan menjaga koneksi. Data yang diterima langsung dimasukkan ke `xQueue`.
*   **Core 1 (TaskLogic):** Mengambil data dari Queue. Jika nilai > Threshold, GPIO Relay dipicu seketika. Untuk pengiriman data ke WiFi, digunakan `xSemaphoreMutex` agar tidak terjadi tabrakan memori saat mengakses *stack* WiFi.

**C. Cloud Backend (Flask API)**
Backend dikembangkan dengan Python Flask. Endpoint `/log` menerima metode POST berisi JSON data. Data disimpan di dalam database berbasis Neon yang dapat diakses menggunakan endpoint `/status`.

#### 2.3 HARDWARE AND SOFTWARE INTEGRATION
Integrasi dilakukan dengan menempelkan Sensor Node pada casing Motor DC menggunakan perekat industrial. Gateway ditempatkan terpisah. Proses *Pairing* antara Sensor dan Gateway terjadi secara otomatis saat *startup* berdasarkan pencocokan UUID Layanan BLE.

---

### CHAPTER 3: TESTING AND EVALUATION

#### 3.1 TESTING
Pengujian dibagi dalam tiga kategori utama:
1.  **Sensor Node Testing**  
Kategori ini menguji bagian noda sensor dengan memverifikasi data mentah yang dibaca, menguji akurasi perhitungan, memvalidasi mekanisme manajemen daya, serta menguji stabilitas pengiriman paket data.
2.  **Gateway Node Testing**  
Kategori ini akan menguji kinerja TaskBLEManager, Memverifikasi TaskAppLogic, menguji responsivitas Relay serta Buzzer, dan melakukan stress test pada jaringan.
3.  **Cloud and Notification Testing**  
Kategori ini akan memverifikasi format pengiriman data, mengecek integritas data, menguji visualisasi data, serta menguji ketahanan mekanisme Mutex.

#### 3.2 RESULT
1.  **Sensor Node Testing**  
Akses register manual pada sensor berhasil membaca data akselerasi, mekanisme manajemen daya berfungsi dengan baik, serta Transmisi data via BLE berjalan lancar.
2.  **Gateway Node Testing**  
TaskBLEManager mampu memindai dan menjaga koneksi, TaskAppLogic berhasil merespons data antrian dengan cepat, Buzzer berhasil aktif dan Fitur Local Intelligence berjalan dengan normal.
3.  **Cloud and Notification Testing**  
Data JSON sesuai dengan template, endpoint berhasil menjalankan tugasnya masing-masing, visualisasi data melalui blynk berhasil, dan Mutex berhasil mencegah konflik data.

#### 3.3 EVALUATION
Secara keseluruhan, sistem berfungsi sesuai rancangan. Penggunaan FreeRTOS sangat krusial dalam memastikan Gateway tidak *hang* saat mencoba menghubungkan ulang WiFi yang putus. Satu kendala yang ditemukan adalah konsumsi daya Sensor Node saat BLE Advertising masih cukup tinggi (~100mA), yang dapat dioptimalkan di masa depan dengan mengatur interval advertising yang lebih jarang.

---

### CHAPTER 4: CONCLUSION

Proyek **IOT22-SmartGuard** berhasil mendemonstrasikan penerapan teknologi IoT tingkat lanjut untuk industri. Dengan mengintegrasikan protokol komunikasi hemat daya (BLE), sistem operasi waktu nyata (FreeRTOS), dan komputasi awan (Cloud), sistem ini menawarkan solusi keamanan mesin yang responsif dan andal. Sistem ini tidak hanya memantau, tetapi juga bertindak secara otonom untuk mencegah kerusakan lebih lanjut, memenuhi standar dasar sistem *Predictive Maintenance*.

---

### REFERENCES
- Random Nerd Tutorials, “ESP32 MPU-6050 Accelerometer and Gyroscope (Arduino) | Random Nerd Tutorials,” Random Nerd Tutorials, Jan. 12, 2021. [Online]. Available: ESP32 MPU-6050 Accelerometer and Gyroscope (Arduino) | Random Nerd Tutorials [Accessed: Dec. 07, 2025]
- Random Nerd Tutorials, “ESP32 I2C Communication: Set Pins, Multiple Bus Interfaces and Peripherals | Random Nerd Tutorials,” Random Nerd Tutorials, Oct. 02, 2019.  [Online]. Available: ESP32 I2C Communication: Set Pins, Multiple Bus Interfaces and Peripherals | Random Nerd Tutorials. [Accessed: Dec. 07, 2025].
Blynk, “Introduction - Blynk Documentation,” Blynk.io, 2022.  [Online]. Available: Introduction | Blynk Documentation. [Accessed: Dec. 08, 2025].
- Espressif Systems,‌ “ESP-BLE-MESH - ESP32 - — ESP-IDF Programming Guide v5.2.3 documentation,” Espressif.com, 2016. [Online]. Available: ESP-BLE-MESH - ESP32 - — ESP-IDF Programming Guide v5.5.1 documentation. [Accessed: Dec. 08, 2025].
- Random Nerd Tutorials, ‌“Arduino Guide for MPU-6050 Accelerometer and Gyroscope | Random Nerd Tutorials,” Random Nerd Tutorials, Feb. 16, 2021. [Online]. Available: Arduino Guide for MPU-6050 Accelerometer and Gyroscope | Random Nerd Tutorials. [Accessed: Dec. 08, 2025].