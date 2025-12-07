from flask import Flask, request, jsonify, render_template_string
from datetime import datetime
from dotenv import load_dotenv
load_dotenv()
import pytz
import os
import psycopg2
from psycopg2.extras import RealDictCursor

app = Flask(__name__)

# Konfigurasi Zona Waktu
jakarta_tz = pytz.timezone('Asia/Jakarta')

# In-Memory Storage (Temporary)
# Catatan: Data akan reset jika server idle lama (sifat Serverless)
system_state = {
    "vibration": 0.0,
    "temperature": 0.0,
    "status": "SAFE",
    "timestamp": "N/A"
}
incident_log = []

# Koneksi ke Database PostgreSQL berbasis NeonDB
def get_db():
    dsn = os.getenv("DATABASE_URL")
    if not dsn:
        raise RuntimeError(
            "DATABASE_URL is not set. Define it in Vercel → Project → Settings → Environment Variables "
            "or a local .env file for development."
        )
    return psycopg2.connect(dsn=dsn, cursor_factory=RealDictCursor)

@app.route('/')
def home():
    return "<h1>IOT22-SmartGuard Backend is Running</h1>"

@app.route('/log', methods=['POST'])
def receive_log():
    try:
        data = request.json
        # Expected JSON: {"vibration": 15.2, "temp": 30.5, "status": "DANGER"}
        if not data:
            return jsonify({"error": "No data provided"}), 400

        current_time = datetime.now(jakarta_tz).strftime("%Y-%m-%d %H:%M:%S")

        system_state["vibration"] = float(data.get("vibration", 0.0))
        system_state["temperature"] = float(data.get("temperature", 0.0))
        system_state["status"] = data.get("status", "UNKNOWN")
        system_state["timestamp"] = current_time

        # Simpan ke log hanya jika status DANGER atau WARNING
        if system_state["status"] in ("DANGER", "WARNING"):
            # Simpan ke database (use NOW() to avoid string->timestamp issues)
            with get_db() as conn, conn.cursor() as cur:
                cur.execute(
                    "INSERT INTO incidents (ts, vibration, temperature, status) VALUES (%s, %s, %s, %s)",
                    (current_time, system_state["vibration"], system_state["temperature"], system_state["status"]),
                )
        return jsonify({"message": "Data logged", "server_time": current_time}), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/status', methods=['GET'])
def get_status():
    accept = request.headers.get("Accept", "")
    try:
        with get_db() as conn, conn.cursor() as cur:
            cur.execute("SELECT * FROM incidents ORDER BY ts DESC LIMIT 50")
            rows = cur.fetchall()
        incident_history = []
        for row in rows:
            ts = row["ts"]
            # Make sure tz-aware before converting
            try:
                if ts.tzinfo is None:
                    import pytz as _p
                    ts = _p.UTC.localize(ts)
                ts_str = ts.astimezone(jakarta_tz).strftime("%Y-%m-%d %H:%M:%S")
            except Exception:
                ts_str = str(ts)
            incident_history.append({
                "timestamp": ts_str,
                "vibration": float(row["vibration"]),
                "temperature": float(row["temperature"]),
                "status": row["status"],
            })
    except Exception:
        incident_history = []
    # If client explicitly requests JSON, keep original behavior
    if "application/json" in accept:
            return jsonify({
                    "current_status": system_state,
                    "incident_history": incident_history
            }), 200

    # Otherwise render a simple HTML dashboard
    template = """
    <!doctype html>
    <html lang="en">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>SmartGuard Status</title>
        <style>
            :root {
                --bg: #0f172a;       /* slate-900 */
                --card: #111827;     /* gray-900 */
                --text: #e5e7eb;     /* gray-200 */
                --muted: #9ca3af;    /* gray-400 */
                --safe: #10b981;     /* emerald-500 */
                --warn: #f59e0b;     /* amber-500 */
                --danger: #ef4444;   /* red-500 */
                --border: #1f2937;   /* gray-800 */
                --accent: #3b82f6;   /* blue-500 */
            }
            body { background: var(--bg); color: var(--text); font-family: system-ui, -apple-system, Segoe UI, Roboto, Ubuntu, Cantarell, Noto Sans, Arial, "Apple Color Emoji", "Segoe UI Emoji"; margin: 0; }
            .container { max-width: 980px; margin: 32px auto; padding: 0 16px; }
            .header { display:flex; align-items:center; gap:12px; margin-bottom: 16px; }
            .brand { font-weight: 700; letter-spacing: .2px; }
            .card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 16px; }
            .grid { display:grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; }
            .label { color: var(--muted); font-size: 13px; }
            .value { font-size: 22px; font-weight: 700; }
            .badge { display:inline-flex; align-items:center; gap:8px; padding: 6px 10px; border-radius: 999px; font-weight: 600; font-size: 13px; }
            .dot { width:10px; height:10px; border-radius:50%; }
            .badge.safe { background: color-mix(in srgb, var(--safe) 18%, transparent); color: var(--safe); }
            .badge.warn { background: color-mix(in srgb, var(--warn) 18%, transparent); color: var(--warn); }
            .badge.danger { background: color-mix(in srgb, var(--danger) 18%, transparent); color: var(--danger); }
            table { width:100%; border-collapse: collapse; }
            th, td { text-align:left; padding:10px 12px; border-bottom: 1px solid var(--border); font-size: 14px; }
            th { color: var(--muted); font-weight: 600; }
            .empty { color: var(--muted); text-align:center; padding: 16px; }
            .footer { margin-top: 16px; color: var(--muted); font-size: 12px; }
            .pill { display:inline-block; padding: 4px 8px; border-radius: 999px; font-weight: 600; font-size: 12px; }
            .pill.safe { background: color-mix(in srgb, var(--safe) 18%, transparent); color: var(--safe); }
            .pill.warn { background: color-mix(in srgb, var(--warn) 18%, transparent); color: var(--warn); }
            .pill.danger { background: color-mix(in srgb, var(--danger) 18%, transparent); color: var(--danger); }
        </style>
    </head>
    <body>
        <div class="container">
            <div class="header">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                    <path d="M12 2l7 4v6c0 5-3.5 9.74-7 10-3.5-.26-7-5-7-10V6l7-4z" stroke="var(--accent)" stroke-width="1.5" fill="none"/>
                </svg>
                <div class="brand">IOT22 • SmartGuard</div>
            </div>

            <div class="grid">
                <div class="card">
                    <div class="label">Current Status</div>
                    {% set s = current_status.status|upper %}
                    {% set cls = "safe" if s == "SAFE" else ("warn" if s == "WARNING" else ("danger" if s == "DANGER" else "")) %}
                    <div class="badge {{ cls }}" style="margin-top:8px;">
                        <span class="dot" style="background: {{ 'var(--safe)' if cls=='safe' else ('var(--warn)' if cls=='warn' else ('var(--danger)' if cls=='danger' else 'var(--muted)')) }};"></span>
                        {{ s }}
                    </div>
                    <div style="margin-top:12px; display:flex; gap:24px;">
                        <div>
                            <div class="label">Last Update</div>
                            <div class="value">{{ current_status.timestamp }}</div>
                        </div>
                        <div>
                            <div class="label">Vibration</div>
                            <div class="value">{{ '%.2f' % current_status.vibration }} g</div>
                        </div>
                        <div>
                            <div class="label">Temperature</div>
                            <div class="value">{{ '%.2f' % current_status.temperature }} °C</div>
                        </div>
                    </div>
                </div>

                <div class="card">
                    <div class="label">Incident History</div>
                    {% if incident_history and incident_history|length > 0 %}
                        <div style="margin-top:8px; max-height: 340px; overflow:auto; border: 1px solid var(--border); border-radius: 8px;">
                            <table>
                                <thead>
                                    <tr>
                                        <th>Time (Asia/Jakarta)</th>
                                        <th>Status</th>
                                        <th>Vibration (g)</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {% for item in incident_history %}
                                        {% set ss = item.status|upper %}
                                        {% set pcl = "pill " + ("safe" if ss == "SAFE" else ("warn" if ss == "WARNING" else ("danger" if ss == "DANGER" else ""))) %}
                                        <tr>
                                            <td>{{ item.timestamp }}</td>
                                            <td><span class="{{ pcl }}">{{ ss }}</span></td>
                                            <td>{{ '%.2f' % item.vibration }}</td>
                                            <td>{{ '%.2f' % item.temperature }} °C</td>
                                        </tr>
                                    {% endfor %}
                                </tbody>
                            </table>
                        </div>
                    {% else %}
                        <div class="empty">No incidents yet. System is stable.</div>
                    {% endif %}
                </div>
            </div>

            <div class="footer">Rendered at server time • Asia/Jakarta</div>
        </div>
    </body>
    </html>
    """

    html = render_template_string(template,
                    current_status=system_state,
                    incident_history=incident_history)
    return html, 200, {"Content-Type": "text/html; charset=utf-8"}

if __name__ == '__main__':
    app.run(debug=True)
