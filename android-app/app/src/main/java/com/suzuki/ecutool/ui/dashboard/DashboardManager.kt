package com.suzuki.ecutool.ui.dashboard

import android.content.Context
import com.google.gson.Gson
import com.suzuki.ecutool.data.DashboardLayout
import com.suzuki.ecutool.data.WidgetConfig
import java.io.File

class DashboardManager(private val context: Context) {
    private val gson = Gson()
    
    var isEditMode: Boolean = false
    var activeSlot: Int = loadLastSlot()
    var activeLayout: DashboardLayout = loadLayout(activeSlot)

    private fun loadLastSlot(): Int {
        return context.getSharedPreferences("dash_prefs", Context.MODE_PRIVATE)
            .getInt("last_slot", 0)
    }

    fun getSlotFile(slot: Int): File {
        return File(context.filesDir, "dashboard_layout_slot_$slot.json")
    }

    fun loadLayout(slot: Int): DashboardLayout {
        activeSlot = slot
        val file = getSlotFile(slot)
        return try {
            if (file.exists()) {
                val json = file.readText()
                gson.fromJson(json, DashboardLayout::class.java)
            } else {
                createDefaultLayout(slot)
            }
        } catch (e: Exception) {
            createDefaultLayout(slot)
        }
    }

    fun saveLayout(slot: Int = activeSlot) {
        try {
            val json = gson.toJson(activeLayout)
            getSlotFile(slot).writeText(json)
            
            context.getSharedPreferences("dash_prefs", Context.MODE_PRIVATE)
                .edit().putInt("last_slot", slot).apply()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun createDefaultLayout(slot: Int): DashboardLayout {
        val layout = DashboardLayout(name = "Layout ${slot + 1}", themeName = "Sport", snapToGrid = true)
        
        // Always build the "Standard Template" regardless of slot index for the reset function
        // Center-focused modern design
        layout.widgets.add(WidgetConfig(
            id = "tach_std", type = "arc", dataSource = "rpm",
            gridX = 3, gridY = 0, gridW = 6, gridH = 8,
            max = 14000f, label = "RPM"
        ))

        layout.widgets.add(WidgetConfig(
            id = "speedo_std", type = "digital", dataSource = "speed",
            gridX = 4, gridY = 3, gridW = 4, gridH = 3,
            label = "KM/H"
        ))

        layout.widgets.add(WidgetConfig(
            id = "gear_std", type = "digital", dataSource = "gear",
            gridX = 9, gridY = 3, gridW = 2, gridH = 3,
            label = "GEAR"
        ))

        layout.widgets.add(WidgetConfig(
            id = "temp_std", type = "bar", dataSource = "coolant",
            gridX = 1, gridY = 2, gridW = 1, gridH = 4,
            max = 120f, label = "TEMP"
        ))

        layout.widgets.add(WidgetConfig(
            id = "neut_std", type = "indicator", dataSource = "neutral",
            gridX = 5, gridY = 0, gridW = 1, gridH = 1, label = "N"
        ))

        return layout
    }

    fun addWidget(config: WidgetConfig) {
        activeLayout.widgets.add(config)
        saveLayout()
    }

    fun removeWidget(id: String) {
        activeLayout.widgets.removeAll { it.id == id }
        saveLayout()
    }

    fun applyPreset(name: String) {
        activeLayout = when (name) {
            "Cyberpunk" -> createCyberpunkLayout()
            "Futuristic" -> createFuturisticLayout()
            "Pip-Boy" -> createPipBoyLayout()
            else -> createDefaultLayout(activeSlot)
        }
        saveLayout()
    }

    private fun createCyberpunkLayout(): DashboardLayout {
        val layout = DashboardLayout(name = "Cyberpunk", themeName = "Race", snapToGrid = false)
        layout.widgets.add(WidgetConfig(
            id = "spd_cp", type = "digital", dataSource = "speed",
            posX = 200f, posY = 200f, gridW = 400, gridH = 300, label = "KM/H"
        ))
        layout.widgets.add(WidgetConfig(
            id = "tach_cp", type = "bar", dataSource = "rpm",
            posX = 50f, posY = 800f, gridW = 1000, gridH = 150, max = 14000f, label = "RPM"
        ))
        return layout
    }

    private fun createFuturisticLayout(): DashboardLayout {
        val layout = DashboardLayout(name = "Futuristic", themeName = "Night", snapToGrid = true)
        layout.widgets.add(WidgetConfig(
            id = "tach_f", type = "arc", dataSource = "rpm",
            gridX = 1, gridY = 1, gridW = 10, gridH = 10, max = 14000f, label = "RPM"
        ))
        return layout
    }

    private fun createPipBoyLayout(): DashboardLayout {
        val layout = DashboardLayout(name = "Pip-Boy", themeName = "Classic", snapToGrid = true)
        layout.widgets.add(WidgetConfig(
            id = "tach_pb", type = "bar", dataSource = "rpm",
            gridX = 0, gridY = 0, gridW = 12, gridH = 2, max = 14000f, label = "RADS"
        ))
        layout.widgets.add(WidgetConfig(
            id = "spd_pb", type = "digital", dataSource = "speed",
            gridX = 4, gridY = 3, gridW = 4, gridH = 4, label = "SND"
        ))
        return layout
    }
}
