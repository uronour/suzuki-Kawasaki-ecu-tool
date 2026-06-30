# OpenCode Project Context — Suzuki ECU Tool

Copy this file + the project to a new machine, then start opencode and say:
"I'm continuing the Suzuki ECU Tool project. Read OPCODE_CONTEXT.md to get up to speed."

---

## Project Overview

K-Line diagnostic tool for Suzuki's SDS protocol on a 2004 GSX-R1000.
Hardware: BIGTREETECH TFT35 V3.0 (STM32F207VCT6, ILI9488 480x320 via FSMC)
WiFi: USR-C322 (USART1, AP mode, TCP Server port 8899, 115200 baud, IP 192.168.1.200)
Android companion app: TCP socket connection to USR-C322, dashboard display

---

## Complete File Structure

### Android App (`android-app/`)
- **`app/src/main/java/com/suzuki/ecutool/`**
  - `EcuToolApp.kt` — Application class, crash handler
  - **`data/`**
    - `DashboardLayout.kt` — `ThemeConfig`, `TickConfig`, `ArcColorSegment`, `WidgetConfig`, `DashboardLayout`
    - `SDSModels.kt` — `SDSData`, `SDSEcuInfo`, `DTC`, `ConnectionState`
  - **`service/`**
    - `TcpConnectionService.kt` — Bound TCP service, auto-reconnect 5s, StateFlow
    - `WifiNetworkBinder.kt` — Binds socket to WiFi network to keep mobile data for internet
  - **`ui/`**
    - `MainActivity.kt` — ViewPager2 + TabLayout, 6 tabs, routes applyPreset() to DashboardEngineFragment
    - `MainViewModel.kt` — Connects service to LiveData<SDSData>, polling, stream
    - `SettingsFragment.kt` — Connection config, preset buttons, toggle switches, polling
    - `DashboardView.kt` — OLD canvas dashboard (unused but kept)
    - `DashboardFragment.kt` — OLD dashboard fragment (unused but kept)
    - `DashConfig.kt` — DashConfig data class + DashConfigManager (SharedPreferences)
    - `GaugeTheme.kt` / `GaugeThemes.kt` — OLD theme enums (unused)
    - `LiveDataFragment.kt`, `DiagnosticsFragment.kt`, `ECUToolsFragment.kt`, `AboutFragment.kt`
    - `PageAdapter.kt`
  - **`ui/dashboard/`**
    - `DashboardEngineFragment.kt` — Widget-based dashboard container, applyPreset(), observes LiveData
    - `DashboardManager.kt` — Loads/saves layout JSON, applyPreset(), loadTheme() from assets, CRUD widgets
    - **`widgets/`**
      - `BaseWidgetView.kt` — Drag/resize in edit mode, `theme` property + `parseColor()` helper
      - `ArcGaugeWidget.kt` — Sweep arc gauge, smooth interpolation, reads ThemeConfig colors
      - `DigitalWidget.kt` — Ghost digits + active value, reads ThemeConfig
      - `BarGaugeWidget.kt` — Vertical bar fill, reads ThemeConfig
      - `StatusIndicatorWidget.kt` — Circle indicator (neutral/fan/MIL etc.), reads ThemeConfig
  - **`util/`**
    - `SDSProtocol.kt` — Text-protocol encoder/parser `key=value`
    - `DTCUtils.kt`
- **`app/src/main/assets/themes/`**
  - `gsxr_factory.json` — K4 OEM (black bg, white text, red needles)
  - `sport.json` — Red-accent sport theme
- **`app/src/main/res/layout/`** — XML layouts for all fragments

### Firmware (`firmware/btt-custom/`)
- PlatformIO project for BIGTREE_TFT35_V3_0
- Custom User/ code: main.c, encoder.c, gauge_ui.c, SDSProtocol.c/h, bt_stream.c, touch_calib.c, etc.
- Flashing: `platformio run --target upload -e BIGTREE_TFT35_V3_0`

### Emulator (`emulator/`)
- MSYS2 gcc + SDL2
- Build: `$env:MSYSTEM="MINGW64"; $env:CHERE_INVOKING="1"; bash -l -c 'mingw32-make'`
- Files: sdl_gfx.c/h, sdl_input.c/h, sim_data.c/h, sim_ili9488_gfx.c, stubs.c/h

### Other
- `docs/SDS_Protocol_Reference.md` — Full protocol datasheet
- `win-app/` — .NET 9 WinForms companion app
- `hardware/` — KiCad boards + wiring docs
- `python/` — Python dashboard

---

## Architecture Decisions

### Gradle / Build System
- AGP 9.2.1, Gradle 9.4.1, built-in Kotlin (no explicit kotlin plugin)
- `gradle.properties` stripped to minimal: `org.gradle.jvmargs`, `android.useAndroidX`, `kotlin.code.style`, `android.nonTransitiveRClass`
- Build: `.\gradlew.bat assembleDebug` from `android-app/`
- Env: `ANDROID_HOME=D:\Android\Sdk`, `JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-17.0.19.10-hotspot`, `GRADLE_USER_HOME=D:\.gradle`
- APK output: `app/build/outputs/apk/debug/app-debug.apk`

### Network Strategy
- **Problem**: Device connects to USR-C322 WiFi AP (no internet). Android routes all traffic through WiFi, losing internet
- **Solution**: `WifiNetworkBinder` uses `ConnectivityManager.getNetworkCapabilities()` to find WiFi Network, then `Network.bindSocket()` to bind only the USR-C322 socket to WiFi
- All other app traffic uses default network (mobile data)
- Android 10+ auto-detects "no internet" on WiFi and keeps mobile data default — explicit binding guarantees it works

### Dashboard System
- **Widget-based**: `DashboardManager` holds `DashboardLayout` with `List<WidgetConfig>`, persisted as JSON in `filesDir/dashboard_layout.json`
- **Theme engine**: JSON files in `assets/themes/`, loaded by `DashboardManager.loadTheme()`, parsed into `ThemeConfig`
- **Presets**: `DashboardManager.applyPreset(name)` creates a new layout + applies theme. Presets: GSX-R Factory, Default Sport, Cyberpunk, Futuristic, Pip-Boy
- **Paint**: Each widget reads colors from `manager.activeLayout.themeConfig` via `theme` property, with `parseColor()` fallback
- **Edit mode**: `isEditMode` toggle, drag to move, resize handle, snap-to-grid, add/remove widgets

### Connection
- `TcpConnectionService` — plain bound service (no foreground), `StateFlow<ConnectionState>`
- 5s auto-reconnect, 15s ping interval, 30s socket timeout
- Command protocol: `key=value` text over TCP

---

## Key Data Classes

```kotlin
// ThemeConfig.kt (in DashboardLayout.kt)
data class ThemeConfig(
    bg, cardBg, textPrimary, textSecondary,
    accent, accent2,
    rpmGreen, rpmYellow, rpmRed,
    needleColor, neutralGreen, hiBeamBlue, warningRed, indicatorBg
)

// WidgetConfig — one per gauge
data class WidgetConfig(
    id, type("arc"/"digital"/"bar"/"indicator"), dataSource,
    gridX/Y/W/H, posX/Y (pixel fallback),
    min, max, label, warningThreshold, redline,
    startAngle, sweepAngle, needleColor, arcColors,
    showTicks, tickConfig, format, fontSize, decimalPlaces
)

// DashboardLayout — the full screen
data class DashboardLayout(
    name, themeName, themeConfig: ThemeConfig?,
    snapToGrid, widgets: MutableList<WidgetConfig>
)
```

---

## Settings Screen Wiring

SettingsFragment has 5 preset buttons (IDs from fragment_settings.xml):
- `presetGsxr` → "GSX-R Factory"
- `presetSport` → "Sport" (falls to createDefaultLayout)
- `presetCyberpunk` → "Cyberpunk"
- `presetPipBoy` → "Pip-Boy"
- `presetFuturistic` → "Futuristic"

Each calls `(activity as MainActivity).applyPreset(name)` → `engineFragment?.applyPreset(name)` → `manager.applyPreset()` + `loadLayout()`

Toggle switches save to `DashConfigManager` (SharedPreferences) directly — not wired to widget visibility yet.

---

## Last Session (26 June 2026)

### What was done:
1. **Fixed Gradle deprecations** — stripped `gradle.properties`, migrated from explicit Kotlin plugin to AGP 9.x built-in Kotlin. Zero warnings.
2. **GSX-R Factory theme** — Created `assets/themes/gsxr_factory.json` (K4 OEM colors), added `loadTheme()`, `applyTheme()`, `createGSXRLayout()` to `DashboardManager`
3. **Widgets read theme colors** — `BaseWidgetView` gained `theme: ThemeConfig?` property + `parseColor()`. All 4 widgets (ArcGauge, Digital, Bar, StatusIndicator) read from `ThemeConfig` with fallback to hardcoded defaults
4. **Wired widget dashboard into app** — `MainActivity` now stores `DashboardEngineFragment` reference, removed old `dashboardFragment`. `applyPreset()` routes to new system
5. **Updated Settings** — Replaced old Theme/Layout buttons with 5 preset buttons (includes GSX-R Factory). Toggle switches save directly to prefs
6. **WifiNetworkBinder** — Socket binding to WiFi network so internet stays alive via mobile data
7. **APK builds clean** — zero warnings, zero errors
8. **FIXED: Fragment lifecycle bug** — `PageAdapter` was pre-creating fragments in a list; `FragmentStateAdapter` destroys/recreates fragments when off-screen, so stored reference went stale. **Fix**: `PageAdapter.createFragment()` now creates fresh instances per position. Fragment communication uses `MainViewModel.triggerPreset()` + `presetSignal` LiveData instead of direct fragment references.
9. **FIXED: Preset routing** — SettingsFragment now calls `viewModel.triggerPreset(name)` → ViewModel _presetSignal LiveData → `DashboardEngineFragment` observes and calls `applyPreset()`. No more `(activity as MainActivity).applyPreset()`.
10. **FIXED: Default layout** — GSX-R Factory is now the built-in default (`createDefaultLayout()` calls `createGSXRLayout()`). First launch shows GSX-R Factory layout with OEM theme.
11. **OTA Update system** — `UpdateManager.kt` checks `suzuki-ecu.servebeer.com/update/version.json`, compares versionCode, downloads APK, triggers install. Settings has "CHECK FOR UPDATE" button.
12. **Python update server** — `server/update_server.py` on port 80, serves `update/version.json` + `update/app-debug.apk`
13. **Version tracking** — versionCode=2, versionName=1.1.0, displayed in Settings footer
14. **FileProvider** set up for APK install (`@xml/file_paths`)

### Files modified in last session:
- `gradle.properties`
- `app/build.gradle.kts` (root + app)
- `app/.../data/DashboardLayout.kt` — `themeConfig` changed to `var`
- `app/.../ui/MainActivity.kt` — rewired to DashboardEngineFragment
- `app/.../ui/SettingsFragment.kt` — replaced theme/layout with preset buttons
- `app/.../ui/dashboard/DashboardManager.kt` — added loadTheme, applyTheme, GSX-R layout
- `app/.../ui/dashboard/widgets/BaseWidgetView.kt` — added theme/parseColor
- `app/.../ui/dashboard/widgets/ArcGaugeWidget.kt` — reads theme colors
- `app/.../ui/dashboard/widgets/DigitalWidget.kt` — reads theme colors
- `app/.../ui/dashboard/widgets/BarGaugeWidget.kt` — reads theme colors
- `app/.../ui/dashboard/widgets/StatusIndicatorWidget.kt` — reads theme colors
- `app/.../service/WifiNetworkBinder.kt` — NEW
- `app/.../service/TcpConnectionService.kt` — wired WifiNetworkBinder
- `app/src/main/assets/themes/gsxr_factory.json` — NEW
- `app/src/main/assets/themes/sport.json` — NEW
- `app/src/main/res/layout/fragment_settings.xml` — updated preset buttons
- `AndroidManifest.xml` — already had required permissions

### OTA Update System
- **Current version**: 1.1.0 (versionCode=2)
- **`UpdateManager.kt`** in `util/` — checks `http://suzuki-ecu.servebeer.com/update/version.json`, compares `versionCode`, downloads APK, triggers install via `FileProvider`
- **Settings** has "CHECK FOR UPDATE" button — triggers check, download, install flow
- **Server**: `server/update_server.py` — Python 3 HTTP server on port 80, serves `update/version.json` and `update/app-debug.apk`
- **To publish update**: build APK → copy to `server/update/app-debug.apk` → update `server/update/version.json` → restart server
- **Manifest**: `REQUEST_INSTALL_PACKAGES` permission, FileProvider at `@xml/file_paths` mapping `updates/` directory
- **Server status**: `http://suzuki-ecu.servebeer.com/` shows version info and download links

### Next steps (in order from user's plan):
1. Make widget views fully use arcColors/segmented RPM zones from ThemeConfig
2. Add more theme JSONs (Carbon Sport, Pip-Boy, Retro Wave, Cyberpunk Neon)
3. Wire toggle switches in Settings to show/hide widgets via DashboardManager
4. Wire the connection status overlay in DashboardEngineFragment
5. When K-Line hardware arrives: test real SDS protocol against ECU

---

## Build Environment Setup

```powershell
$env:JAVA_HOME="C:\Program Files\Eclipse Adoptium\jdk-17.0.19.10-hotspot"
$env:ANDROID_HOME="D:\Android\Sdk" 
$env:GRADLE_USER_HOME="D:\.gradle"
cd android-app
.\gradlew.bat assembleDebug
# APK at: app/build/outputs/apk/debug/app-debug.apk
```

For firmware flashing (ST-Link connected):
```powershell
cd firmware/btt-custom
platformio run --target upload -e BIGTREE_TFT35_V3_0
```

Git remote: `origin = https://github.com/uronour/suzuki-ecu-tool.git`

---

## Important Reminders for AI

- Do NOT modify original on D: drive — clone/backup to F: or E: for work
- The old `DashboardFragment` + `DashboardView` + `GaugeThemes` are dead code — kept but not shown
- `g_sdsData` is `SDSData` in sim_data.c but `SDS_Data` in SDSProtocol.h — EMULATOR_BUILD guard selects correct header
- Touch calibration stored at flash `0x08004000`
- GH CLI PAT lacks release scope — releases postponed
- Crash log: `/data/data/com.suzuki.ecutool/files/crash_log.txt`
- Dash RPM zones: 0-8000 green, 8000-12000 yellow, 12000-15500 red
