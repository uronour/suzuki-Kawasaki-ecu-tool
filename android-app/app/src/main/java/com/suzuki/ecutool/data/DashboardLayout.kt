package com.suzuki.ecutool.data

import com.google.gson.annotations.SerializedName

/**
 * Configuration for a single gauge or display element on the dashboard.
 */
data class WidgetConfig(
    @SerializedName("id") val id: String,
    @SerializedName("type") val type: String,          // "arc", "digital", "bar", "indicator"
    @SerializedName("source") val dataSource: String,  // "rpm", "speed", "gear", "coolant", etc.
    @SerializedName("gridX") var gridX: Int = 0,       // 0-11 for 12-column grid
    @SerializedName("gridY") var gridY: Int = 0,
    @SerializedName("gridW") var gridW: Int = 2,
    @SerializedName("gridH") var gridH: Int = 2,
    @SerializedName("pixelX") var posX: Float = 0f,    // Used if snapToGrid is false
    @SerializedName("pixelY") var posY: Float = 0f,
    @SerializedName("label") val label: String? = null,
    @SerializedName("min") val min: Float = 0f,
    @SerializedName("max") val max: Float = 100f,
    @SerializedName("warn") val warningThreshold: Float? = null
)

/**
 * Entire dashboard layout configuration.
 */
data class DashboardLayout(
    @SerializedName("name") val name: String = "Default",
    @SerializedName("theme") var themeName: String = "Sport",
    @SerializedName("snap") var snapToGrid: Boolean = true,
    @SerializedName("widgets") val widgets: MutableList<WidgetConfig> = mutableListOf()
)
