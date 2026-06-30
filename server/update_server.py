"""
Suzuki ECU Tool OTA Update Server
Automatically detects new APKs and updates version.json.
Uses correct MIME types to prevent download hangs.

Usage:
  python update_server.py [port]
"""

import json
import os
import sys
import time
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from threading import Thread

# Reverting to Port 80 as requested
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 80
UPDATE_DIR = Path(__file__).parent / "update"
APK_NAME = "app-debug.apk"
VERSION_FILE = "version.json"
LOG_FILE = Path(__file__).parent / "server_log.txt"

def log(msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{timestamp}] {msg}"
    print(line)
    try:
        with open(LOG_FILE, "a") as f:
            f.write(line + "\n")
    except:
        pass

def auto_update_manifest():
    """Watches the APK file and updates version.json if APK is newer."""
    log("Auto-Manifest Watcher started...")
    last_mtime = 0
    while True:
        apk_path = UPDATE_DIR / APK_NAME
        if apk_path.exists():
            mtime = apk_path.stat().st_mtime
            if mtime > last_mtime:
                ts = time.localtime(mtime)
                version_code = int(time.strftime("%y%m%d%H", ts))
                version_name = f"1.1.{time.strftime('%m%d', ts)}"

                manifest = {
                    "versionName": version_name,
                    "versionCode": version_code,
                    "url": APK_NAME
                }

                with open(UPDATE_DIR / VERSION_FILE, "w") as f:
                    json.dump(manifest, f, indent=2)

                last_mtime = mtime
                log(f"New APK detected! Manifest updated: {version_name} ({version_code})")

        time.sleep(10)

class UpdateHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(UPDATE_DIR), **kwargs)

    def log_message(self, format, *args):
        log(f"Request from {self.client_address[0]}: {format%args}")

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        super().end_headers()

    def do_GET(self):
        # Force correct MIME type and headers for APK files to prevent hangs
        if self.path.endswith(".apk"):
            apk_path = UPDATE_DIR / APK_NAME
            if apk_path.exists():
                stat = apk_path.stat()
                self.send_response(200)
                self.send_header("Content-Type", "application/vnd.android.package-archive")
                self.send_header("Content-Length", str(stat.st_size))
                self.send_header("Content-Disposition", f'attachment; filename="{APK_NAME}"')
                self.end_headers()
                with open(apk_path, 'rb') as f:
                    self.wfile.write(f.read())
                return

        if self.path == "/":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self._print_status_page()
        else:
            super().do_GET()

    def _print_status_page(self):
        version_file = UPDATE_DIR / VERSION_FILE
        apk_file = UPDATE_DIR / APK_NAME

        if version_file.exists():
            try:
                info = json.loads(version_file.read_text())
                version = info.get("versionName", "?")
                version_code = info.get("versionCode", "?")
            except:
                version = "Error"
                version_code = "Error"
        else:
            version = "N/A"
            version_code = "N/A"

        apk_size = apk_file.stat().st_size if apk_file.exists() else 0
        apk_exists = "Yes" if apk_file.exists() else "No"

        html = f"""<!DOCTYPE html>
<html>
<head><title>Suzuki ECU Tool - Update Server</title>
<style>
body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background: #000; color: #fff; padding: 2em; text-align: center; }}
.container {{ max-width: 600px; margin: 0 auto; background: #111; padding: 20px; border-radius: 12px; border: 1px solid #333; }}
h1 {{ color: #FF9800; margin-bottom: 0.5em; }}
table {{ width: 100%; border-collapse: collapse; margin: 1.5em 0; text-align: left; }}
td, th {{ padding: 12px; border-bottom: 1px solid #222; }}
th {{ color: #888; font-weight: normal; width: 40%; }}
.status {{ color: #4CAF50; font-weight: bold; }}
.btn {{ display: inline-block; background: #FF9800; color: #000; padding: 12px 24px; text-decoration: none; border-radius: 6px; font-weight: bold; margin-top: 10px; }}
</style>
</head>
<body>
<div class="container">
<h1>Suzuki ECU Tool</h1>
<p>Update Service Status: <span class="status">ONLINE</span></p>
<table>
<tr><th>Latest Version</th><td>{version}</td></tr>
<tr><td>Version Code</td><td>{version_code}</td></tr>
<tr><td>Build Status</td><td>Ready ({apk_size / 1024 / 1024:.2f} MB)</td></tr>
</table>
<a href="/{APK_NAME}" class="btn">DOWNLOAD LATEST APK</a>
<p style="color: #555; font-size: 12px; margin-top: 20px;">Server Port: {PORT}</p>
</div>
</body>
</html>"""
        self.wfile.write(html.encode())

if __name__ == "__main__":
    if not UPDATE_DIR.exists():
        UPDATE_DIR.mkdir(parents=True)

    watcher = Thread(target=auto_update_manifest, daemon=True)
    watcher.start()

    os.chdir(UPDATE_DIR.parent)
    server = HTTPServer(("0.0.0.0", PORT), UpdateHandler)
    log(f"Update Server Started: http://suzuki-ecu.servebeer.com:{PORT}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log("Server stopped")
        server.server_close()
