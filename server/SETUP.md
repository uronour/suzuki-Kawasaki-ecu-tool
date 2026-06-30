# Suzuki ECU Tool — OTA Update Server Setup

This guide explains how to set up your own OTA (Over-The-Air) update server for the Suzuki ECU Tool Android app.

## What you need

- A publicly accessible HTTP/HTTPS server (can be a VPS, Raspberry Pi, your PC with port forwarding, etc.)
- Python 3.x (for the provided simple server) OR any web server (nginx, Apache, etc.)

## Quick Start with Python Server

```bash
cd server
python3 update_server.py
```

This starts a server on port 80. The app checks `http://your-server/update/version.json`.

## Server Structure

```
server/
├── update_server.py       # Simple Python HTTP server (optional)
└── update/
    ├── version.json       # Update manifest (required)
    └── app-debug.apk      # The APK file (required)
```

## Required: `version.json`

The app checks this file for updates. Format:

```json
{
  "versionCode": 2,
  "versionName": "1.1.0",
  "apkUrl": "http://your-server/update/app-debug.apk",
  "changelog": "What's new in this version"
}
```

**Fields:**
- `versionCode` (int): Must increment with each build. Android uses this for upgrade logic.
- `versionName` (string): Human-readable version (semver recommended).
- `apkUrl` (string): Direct download URL for the APK.
- `changelog` (string, optional): Displayed in the update notification.

## Setting Updating anifest for the App

In `app/build.gradle.kts`, increment on each build:

```kotlin
defaultConfig {
    versionCode = 2      // Increment this
    versionName = "1.1.0"  // Update this
}
```

## Publishing a New Update

1. Build the APK:
   ```bash
   cd android-app
   ./gradlew.bat assembleDebug
   ```

2. Copy to server:
   ```bash
   cp android-app/app/build/outputs/apk/debug/app-debug.apk server/update/
   ```

3. Update `server/update/version.json` with new versionCode and versionName.

4. Restart your server (if using Python server).

## Custom Update URL in the App

The update URL is obfuscated in the code. To use your own server:

1. **Option A — Edit source (for your builds):**
   - Update `UrlObfuscator.kt` with your encoded URL
   - Use the encoding script below

2. **Option B — Runtime config (recommended):**
   - The app reads from `filesDir/update_config.json` if it exists
   - Create this file on the device:
     ```json
     {
       "updateUrl": "http://your-server/update/version.json"
     }
     ```

## Encoding Your Own URL

Run this Python script to encode your URL:

```python
key = b"SuzukiECU2026"
url = b"http://your-server/update/version.json"
encoded = bytes(a ^ b for a, b in zip(url, key * (len(url)//len(key) + 1)))
print(''.join(f'{b:02x}' for b in encoded))
```

Replace `ENCODED_UPDATE_URL_HEX` in `UrlObfuscator.kt` with the output.

## Android App Permissions

The app requires:
- `INTERNET` — to check and download updates
- `REQUEST_INSTALL_PACKAGES` — to trigger APK installation (Android 8+)

FileProvider is configured for secure APK sharing.

## Testing Locally

1. Start server: `python3 update_server.py`
2. Install APK on device
3. In app Settings → "CHECK FOR UPDATE"
4. If versionCode is newer, APK downloads and install dialog appears

## Notes

- **Never commit real URLs to git** — use `.gitignore` for config files
- Use HTTPS in production (the Python server is HTTP only)
- For production, use a proper web server (nginx) with TLS
- The app only downloads from the URL in `version.json` — ensure it points to the APK on your server
