
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

*(Space untuk gambar skematik/diagram blok sistem)*

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
Backend dikembangkan dengan Python Flask. Endpoint `/api/log` menerima metode POST berisi JSON data. Data disimpan sementara dalam struktur data list (Python List) yang dapat diakses kembali melalui endpoint GET `/api/status`.

#### 2.3 HARDWARE AND SOFTWARE INTEGRATION
Integrasi dilakukan dengan menempelkan Sensor Node pada casing Motor DC menggunakan perekat industrial. Gateway ditempatkan terpisah. Proses *Pairing* antara Sensor dan Gateway terjadi secara otomatis saat *startup* berdasarkan pencocokan UUID Layanan BLE.

---

### CHAPTER 3: TESTING AND EVALUATION

#### 3.1 TESTING
Pengujian dilakukan dalam tiga skenario utama:
1.  **Normal Operation:** Motor dijalankan pada kecepatan normal. Getaran terukur rata-rata 0.5 - 2.0 m/s². Relay tetap ON.
2.  **Failure Simulation:** Beban tidak seimbang diberikan pada poros motor untuk menghasilkan getaran hebat.
3.  **Connectivity Stress Test:** Router WiFi dimatikan saat sistem berjalan untuk menguji fungsi *Local Intelligence*.

#### 3.2 RESULT
*   **Respon Keselamatan:** Pada simulasi kegagalan, Gateway berhasil mematikan motor dalam waktu rata-rata **850 milidetik** setelah getaran melonjak di atas 5.5 m/s².
*   **Kestabilan Koneksi:** BLE stabil hingga jarak 8 meter tanpa halangan.
*   **Cloud Logging:** Data kejadian "DANGER" berhasil muncul di respon API Flask dengan timestamp yang akurat.
*   **Ketahanan Sistem:** Saat WiFi dimatikan, fungsi Relay dan Buzzer **tetap bekerja** normal berkat arsitektur FreeRTOS yang memisahkan logika kontrol dan logika jaringan.

#### 3.3 EVALUATION
Secara keseluruhan, sistem berfungsi sesuai rancangan. Penggunaan FreeRTOS sangat krusial dalam memastikan Gateway tidak *hang* saat mencoba menghubungkan ulang WiFi yang putus. Satu kendala yang ditemukan adalah konsumsi daya Sensor Node saat BLE Advertising masih cukup tinggi (~100mA), yang dapat dioptimalkan di masa depan dengan mengatur interval advertising yang lebih jarang.

---

### CHAPTER 4: CONCLUSION

Proyek **IOT22-SmartGuard** berhasil mendemonstrasikan penerapan teknologi IoT tingkat lanjut untuk industri. Dengan mengintegrasikan protokol komunikasi hemat daya (BLE), sistem operasi waktu nyata (FreeRTOS), dan komputasi awan (Cloud), sistem ini menawarkan solusi keamanan mesin yang responsif dan andal. Sistem ini tidak hanya memantau, tetapi juga bertindak secara otonom untuk mencegah kerusakan lebih lanjut, memenuhi standar dasar sistem *Predictive Maintenance*.

---

### REFERENCES
1.  *FreeRTOS Documentation* (2024). "Task Management & Queues". Amazon Web Services.
2.  *ESP32 Technical Reference Manual*. Espressif Systems.
3.  *Blynk IoT Platform Documentation*.
4.  Modul Praktikum IoT Laboratorium DTE FTUI (2024).