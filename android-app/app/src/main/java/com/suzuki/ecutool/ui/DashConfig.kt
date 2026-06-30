package com.suzuki.ecutool.ui

import android.content.Context
import android.content.SharedPreferences

data class DashConfig(
    val showRpmGauge: Boolean = true,
    val showSpeedo: Boolean = true,
    val showGear: Boolean = true,
    val showClock: Boolean = true,
    val showOdometer: Boolean = true,
    val showFuelGauge: Boolean = true,
    val showCenterPanel: Boolean = true,
    val showIndicators: Boolean = true,
    val showNeutral: Boolean = true,
    val showFan: Boolean = true,
    val showClutch: Boolean = true,
    val showSidestand: Boolean = true,
    val showCheckEngine: Boolean = true,
    val showHiBeam: Boolean = true,
    val showTurnSignals: Boolean = true,
    val showOilLight: Boolean = true,
    val showKmLarge: Boolean = true,
    val themeName: String = "Sport",
    val layoutName: String = "Sport",
    val unitKmh: Boolean = true,
    val shiftRpm: Int = 12500
)

class DashConfigManager(context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun load(): DashConfig = DashConfig(
        showRpmGauge = prefs.getBool("rpm", true),
        showSpeedo = prefs.getBool("speedo", true),
        showGear = prefs.getBool("gear", true),
        showClock = prefs.getBool("clock", true),
        showOdometer = prefs.getBool("odo", true),
        showFuelGauge = prefs.getBool("fuel", true),
        showCenterPanel = prefs.getBool("panel", true),
        showIndicators = prefs.getBool("indicators", true),
        showNeutral = prefs.getBool("neutral", true),
        showFan = prefs.getBool("fan", true),
        showClutch = prefs.getBool("clutch", true),
        showSidestand = prefs.getBool("sidestand", true),
        showCheckEngine = prefs.getBool("check_eng", true),
        showHiBeam = prefs.getBool("hibeam", true),
        showTurnSignals = prefs.getBool("turns", true),
        showOilLight = prefs.getBool("oil", true),
        showKmLarge = prefs.getBool("km_large", true),
        themeName = prefs.getString("theme", "Sport") ?: "Sport",
        layoutName = prefs.getString("layout", "Sport") ?: "Sport",
        unitKmh = prefs.getBool("unit_kmh", true),
        shiftRpm = prefs.getInt("shift_rpm", 12500)
    )

    fun save(config: DashConfig) {
        prefs.edit()
            .putBool("rpm", config.showRpmGauge)
            .putBool("speedo", config.showSpeedo)
            .putBool("gear", config.showGear)
            .putBool("clock", config.showClock)
            .putBool("odo", config.showOdometer)
            .putBool("fuel", config.showFuelGauge)
            .putBool("panel", config.showCenterPanel)
            .putBool("indicators", config.showIndicators)
            .putBool("neutral", config.showNeutral)
            .putBool("fan", config.showFan)
            .putBool("clutch", config.showClutch)
            .putBool("sidestand", config.showSidestand)
            .putBool("check_eng", config.showCheckEngine)
            .putBool("hibeam", config.showHiBeam)
            .putBool("turns", config.showTurnSignals)
            .putBool("oil", config.showOilLight)
            .putBool("km_large", config.showKmLarge)
            .putString("theme", config.themeName)
            .putString("layout", config.layoutName)
            .putBool("unit_kmh", config.unitKmh)
            .putInt("shift_rpm", config.shiftRpm)
            .apply()
    }

    fun setTheme(name: String) { prefs.edit().putString("theme", name).apply() }
    fun getThemeName(): String = prefs.getString("theme", "Sport") ?: "Sport"
    
    fun setLayout(name: String) { prefs.edit().putString("layout", name).apply() }
    fun getLayoutName(): String = prefs.getString("layout", "Sport") ?: "Sport"

    private fun SharedPreferences.getBool(key: String, default: Boolean): Boolean =
        getBoolean(key, default)

    private fun SharedPreferences.Editor.putBool(key: String, value: Boolean) =
        putBoolean(key, value)

    companion object {
        private const val PREFS_NAME = "dash_config"
    }
}
