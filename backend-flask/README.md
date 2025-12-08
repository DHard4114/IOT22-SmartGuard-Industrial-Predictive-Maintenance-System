# Backend Flask — IOT22 SmartGuard

Dokumen ini menjelaskan fungsi dan cara penggunaan layanan backend yang dibangun dengan Flask untuk sistem Industrial Predictive Maintenance (SmartGuard).

## Ringkasan
- Backend menerima data getaran dan status dari perangkat IoT (gateway/sensor).
- Menyimpan insiden (WARNING/DANGER) ke database PostgreSQL (NeonDB).
- Menyajikan status terkini dan riwayat insiden dalam format JSON atau halaman HTML sederhana.
- Siap di-deploy pada Vercel sebagai Serverless Function.

## Fitur Utama
- Pengaturan zona waktu `Asia/Jakarta` untuk penandaan waktu.
- Endpoint `/log` untuk menerima data dan menyimpan insiden ke database.
- Endpoint `/status` untuk melihat status terkini dan riwayat insiden (JSON/HTML).
- Konfigurasi environment menggunakan variabel `DATABASE_URL` (disarankan via `.env` atau Vercel Env).

## Endpoint API
- `GET /` — Cek apakah backend aktif.
- `POST /log` — Kirim data dari perangkat:
	- Body JSON contoh: `{ "vibration": 15.2, "status": "DANGER" }`
	- Perilaku: memperbarui `system_state`; jika status `WARNING` atau `DANGER`, melakukan `INSERT` ke tabel `incidents`.
- `GET /status` — Mendapatkan status:
	- Jika header `Accept: application/json`, mengembalikan JSON:
		- `current_status`: status terakhir.
		- `incident_history`: daftar insiden terbaru.
	- Jika tidak, mengembalikan halaman HTML dengan badge status dan tabel riwayat.

## Konfigurasi Environment (.env)
- Variabel yang diperlukan: `DATABASE_URL` berisi koneksi Postgres (NeonDB).
- Contoh `.env` (lokal):
```
DATABASE_URL=postgresql://USER:PASS@HOST/neondb?sslmode=require
```
- Di Vercel: tambahkan `DATABASE_URL` pada Project → Settings → Environment Variables untuk scope `Production`, `Preview`, dan `Development`.

## Skema Database (Contoh)
Gunakan skema berbasis timestamp dan nilai getaran/status:
```sql
CREATE TABLE incidents (
	id SERIAL PRIMARY KEY,
	vibration NUMERIC(10,3) NOT NULL,
	status TEXT NOT NULL,
	ts TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX incidents_ts_idx ON incidents (ts DESC);
```

Jika kode Anda menggunakan kolom `timestamp` (tanpa default), pastikan tipe dan konversi sesuai saat `SELECT` dan `INSERT`.

## Menjalankan Secara Lokal (Windows PowerShell)
```powershell
cd D:\Referensi_Belajar\Semester_5\IOT\Finpro\backend-flask
python -m pip install -r requirements.txt
# Pastikan DATABASE_URL tersedia
$env:DATABASE_URL = "postgresql://USER:PASS@HOST/neondb?sslmode=require"
python api/index.py
# Buka http://127.0.0.1:5000/status
```

## Deploy ke Vercel
- Pastikan `requirements.txt` berisi dependensi:
	- Flask, python-dotenv, pytz, psycopg2-binary
- Tambahkan `vercel.json` bila perlu untuk menentukan runtime Python dan routing.
- Set `DATABASE_URL` di Vercel Environment.
- Deploy:
```powershell
vercel --prod
```

## Troubleshooting
- `ModuleNotFoundError: No module named 'dotenv'` → Pastikan `python-dotenv` ada di `requirements.txt`.
- `DATABASE_URL is not set` → Cek `.env`, `load_dotenv()`, dan variabel env di Vercel.
- Gagal menulis ke DB → Pastikan `INSERT` diikuti `commit()` (atau gunakan context manager `with`), cek kredensial Neon dan hak akses.
- Zona waktu/format waktu → Konversi `TIMESTAMPTZ` ke `Asia/Jakarta` sebelum ditampilkan.

## Struktur Proyek (Ringkas)
```
backend-flask/
	api/
		index.py        # Flask app (endpoint /, /log, /status)
	requirements.txt  # Dependensi Python
	vercel.json       # (opsional) konfigurasi Vercel
	README.md         # Dokumen ini
```

## Catatan Keamanan
- Jangan commit kredensial database ke repo.
- Gunakan variabel lingkungan di produksi.
- Pertimbangkan rotasi password jika terjadi kebocoran.

