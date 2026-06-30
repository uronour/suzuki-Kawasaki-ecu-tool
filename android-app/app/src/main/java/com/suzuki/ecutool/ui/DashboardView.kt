package com.suzuki.ecutool.ui

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View
import com.suzuki.ecutool.data.SDSData
import java.util.*
import kotlin.math.*

enum class DashLayout {
    SPORT,   // Circular big tach, digital speedo
    RACE,    // Horizontal bar tach
    CLASSIC  // Analog dual-gauge style
}

data class GaugeTheme(
    val name: String,
    val bgColor: Int,
    val cardBg: Int,
    val textPrimary: Int,
    val textSecondary: Int,
    val accent: Int,
    val accent2: Int,
    val rpmGreen: Int,
    val rpmYellow: Int,
    val rpmRed: Int,
    val needleColor: Int,
    val indicatorBg: Int,
    val warningRed: Int
)

object GaugeThemes {
    val Sport = GaugeTheme("Sport",
        Color.BLACK, Color.rgb(20, 20, 30),
        Color.WHITE, Color.rgb(180, 180, 190),
        Color.rgb(220, 20, 20), Color.rgb(0, 150, 255),
        Color.rgb(0, 255, 100), Color.rgb(255, 200, 0), Color.rgb(255, 0, 0),
        Color.RED, Color.rgb(30, 30, 45), Color.RED)

    val Race = GaugeTheme("Race",
        Color.rgb(10, 10, 10), Color.rgb(30, 30, 30),
        Color.rgb(0, 255, 255), Color.rgb(150, 255, 255),
        Color.rgb(255, 255, 0), Color.rgb(255, 255, 255),
        Color.rgb(0, 255, 0), Color.rgb(255, 255, 0), Color.rgb(255, 0, 0),
        Color.WHITE, Color.rgb(40, 40, 40), Color.RED)

    val Classic = GaugeTheme("Classic",
        Color.rgb(20, 20, 15), Color.rgb(40, 40, 35),
        Color.rgb(240, 235, 220), Color.rgb(180, 175, 160),
        Color.rgb(210, 160, 80), Color.rgb(230, 190, 120),
        Color.rgb(120, 200, 100), Color.rgb(220, 180, 70), Color.rgb(220, 80, 60),
        Color.rgb(210, 160, 80), Color.rgb(35, 35, 30), Color.rgb(220, 60, 40))

    val Night = GaugeTheme("Night",
        Color.rgb(4, 7, 14), Color.rgb(10, 16, 28),
        Color.rgb(170, 215, 255), Color.rgb(90, 140, 195),
        Color.rgb(0, 175, 255), Color.rgb(55, 195, 255),
        Color.rgb(0, 200, 200), Color.rgb(0, 175, 255), Color.rgb(255, 45, 95),
        Color.rgb(0, 195, 255), Color.rgb(14, 20, 34), Color.rgb(255, 45, 95))
}

class TripComputer {
    var tripKm: Float = 0f
    var odometerKm: Float = 12500f
    var avgSpeed: Float = 0f
    var maxSpeed: Float = 0f
    var movingTime: Long = 0
    var fuelUsedMl: Float = 0f
    private var lastSpeed: Float = 0f
    private var lastTime: Long = 0

    fun update(speedKmh: Float, injectorMs: Float, nowMs: Long) {
        if (lastTime == 0L) { lastTime = nowMs; return }
        val dt = (nowMs - lastTime) / 1000f
        if (dt <= 0f || dt > 5f) { lastTime = nowMs; lastSpeed = speedKmh; return }

        val avgSpd = (speedKmh + lastSpeed) / 2f
        val distKm = avgSpd * dt / 3600f
        tripKm += distKm
        odometerKm += distKm
        if (speedKmh > maxSpeed) maxSpeed = speedKmh
        if (speedKmh > 0) movingTime += (dt * 1000).toLong()
        fuelUsedMl += injectorMs * dt * 0.003f
        lastSpeed = speedKmh; lastTime = nowMs
    }

    fun resetTrip() { tripKm = 0f; maxSpeed = 0f; movingTime = 0; fuelUsedMl = 0f }
}

class DashboardView(context: Context, attrs: AttributeSet? = null) : View(context, attrs) {

    var data = SDSData()
        set(v) { field = v; postInvalidateOnAnimation() }

    var trip = TripComputer()
    var config = DashConfig()
    var theme: GaugeTheme = GaugeThemes.Sport
    var layoutMode: DashLayout = DashLayout.SPORT

    // Smoothing & Peaks
    private var displayRpm = 0f
    private var displaySpeed = 0f
    private var peakRpm = 0f
    private var peakSpeed = 0f
    
    // Indicators
    var leftTurnBlinking = false
    var rightTurnBlinking = false
    var hiBeamOn = false
    var hasDtc = false

    private val pFill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val pStroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND }
    private val pText = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Typeface.DEFAULT_BOLD }
    private val rect = RectF()

    override fun onDraw(canvas: Canvas) {
        val w = width.toFloat(); val h = height.toFloat()
        val cx = w / 2f; val cy = h / 2f; val s = minOf(w, h)

        // Smooth movement (simple lerp)
        displayRpm = displayRpm * 0.7f + data.rpm * 0.3f
        displaySpeed = displaySpeed * 0.8f + data.speedKmh() * 0.2f
        
        if (data.rpm > peakRpm) peakRpm = data.rpm.toFloat()
        if (data.speedKmh() > peakSpeed) peakSpeed = data.speedKmh()

        canvas.drawColor(theme.bgColor)

        when (layoutMode) {
            DashLayout.SPORT -> drawSportLayout(canvas, cx, cy, s)
            DashLayout.RACE -> drawRaceLayout(canvas, w, h, s)
            DashLayout.CLASSIC -> drawClassicLayout(canvas, cx, cy, s)
        }
        
        if (config.showIndicators) drawUniversalIndicators(canvas, w, h, s)
    }

    private fun drawSportLayout(canvas: Canvas, cx: Float, cy: Float, s: Float) {
        val r = s * 0.42f
        val startAngle = 140f
        val sweepAngle = 260f
        val maxRpm = 14000f
        val rpmAngle = (displayRpm / maxRpm) * sweepAngle

        rect.set(cx - r, cy - r, cx + r, cy + r)
        pStroke.strokeWidth = s * 0.08f
        pStroke.color = theme.cardBg
        canvas.drawArc(rect, startAngle, sweepAngle, false, pStroke)

        pStroke.strokeWidth = s * 0.06f
        pStroke.color = theme.rpmGreen
        canvas.drawArc(rect, startAngle, min(rpmAngle, sweepAngle * 0.6f), false, pStroke)
        
        if (rpmAngle > sweepAngle * 0.6f) {
            pStroke.color = theme.rpmYellow
            canvas.drawArc(rect, startAngle + sweepAngle * 0.6f, min(rpmAngle - sweepAngle * 0.6f, sweepAngle * 0.25f), false, pStroke)
        }
        if (rpmAngle > sweepAngle * 0.85f) {
            pStroke.color = theme.rpmRed
            canvas.drawArc(rect, startAngle + sweepAngle * 0.85f, rpmAngle - sweepAngle * 0.85f, false, pStroke)
        }

        // Shift Light
        if (data.rpm >= config.shiftRpm) {
            if (System.currentTimeMillis() % 150 < 75) {
                pFill.color = Color.WHITE
                canvas.drawCircle(cx, cy - r - s * 0.12f, s * 0.06f, pFill)
                pFill.maskFilter = BlurMaskFilter(20f, BlurMaskFilter.Blur.NORMAL)
                canvas.drawCircle(cx, cy - r - s * 0.12f, s * 0.08f, pFill)
                pFill.maskFilter = null
            }
        }

        // Peak RPM Marker
        val peakAngle = startAngle + (peakRpm / maxRpm) * sweepAngle
        pStroke.color = Color.WHITE
        pStroke.strokeWidth = 4f
        canvas.drawArc(rect, peakAngle - 0.5f, 1f, false, pStroke)

        pText.textSize = s * 0.04f
        pText.textAlign = Paint.Align.CENTER
        for (i in 0..14) {
            val angle = startAngle + (i / 14f) * sweepAngle
            val rad = Math.toRadians(angle.toDouble())
            val labelR = r - s * 0.12f
            pText.color = theme.textPrimary
            canvas.drawText("$i", cx + labelR * cos(rad).toFloat(), cy + labelR * sin(rad).toFloat() + s * 0.015f, pText)
        }

        pText.textSize = s * 0.22f
        pText.color = theme.textPrimary
        canvas.drawText("%.0f".format(displaySpeed), cx, cy + s * 0.05f, pText)
        pText.textSize = s * 0.05f
        pText.color = theme.textSecondary
        canvas.drawText(if(config.unitKmh) "km/h" else "mph", cx, cy + s * 0.12f, pText)

        drawGearLarge(canvas, cx + s * 0.25f, cy + s * 0.25f, s * 0.15f)
        drawInfoPanel(canvas, cx - s * 0.45f, cy + s * 0.1f, s, "TEMP", "${data.coolantTemp} °C")
        drawInfoPanel(canvas, cx + s * 0.45f, cy + s * 0.1f, s, "VOLT", "%.1f V".format(data.batteryVoltage()))
    }

    private fun drawRaceLayout(canvas: Canvas, w: Float, h: Float, s: Float) {
        val barTop = h * 0.1f
        val barHeight = h * 0.12f
        val barWidth = w * 0.9f
        val barLeft = (w - barWidth) / 2f
        
        rect.set(barLeft, barTop, barLeft + barWidth, barTop + barHeight)
        pFill.color = theme.cardBg
        canvas.drawRoundRect(rect, 10f, 10f, pFill)
        
        val rpmPercent = (displayRpm / 14000f).coerceIn(0f, 1f)
        val fillWidth = barWidth * rpmPercent
        
        val shader = LinearGradient(barLeft, 0f, barLeft + barWidth, 0f,
            intArrayOf(theme.rpmGreen, theme.rpmYellow, theme.rpmRed),
            floatArrayOf(0.5f, 0.8f, 1.0f), Shader.TileMode.CLAMP)
        pFill.shader = shader
        canvas.drawRoundRect(barLeft, barTop, barLeft + fillWidth, barTop + barHeight, 10f, 10f, pFill)
        pFill.shader = null

        pText.textSize = h * 0.4f
        pText.textAlign = Paint.Align.LEFT
        pText.color = theme.textPrimary
        canvas.drawText("%.0f".format(displaySpeed), barLeft, h * 0.65f, pText)
        
        drawGearLarge(canvas, w - barLeft - s * 0.3f, h * 0.55f, s * 0.3f)
        
        val rowY = h * 0.75f
        drawInfoPanel(canvas, barLeft + s * 0.1f, rowY, s, "TPS", "${data.throttlePos}%")
        drawInfoPanel(canvas, barLeft + s * 0.3f, rowY, s, "MAP", "${data.mapKpa} kPa")
        drawInfoPanel(canvas, barLeft + s * 0.5f, rowY, s, "IAT", "${data.intakeAirTemp} °C")
    }

    private fun drawClassicLayout(canvas: Canvas, cx: Float, cy: Float, s: Float) {
        val gaugeR = s * 0.35f
        drawAnalogGauge(canvas, cx - s * 0.3f, cy, gaugeR, "RPM", displayRpm, 14000f, theme.rpmRed)
        drawAnalogGauge(canvas, cx + s * 0.3f, cy, gaugeR, if(config.unitKmh) "km/h" else "mph", displaySpeed, 300f, theme.accent2)
        
        pText.textSize = s * 0.04f
        pText.textAlign = Paint.Align.CENTER
        pText.color = theme.textPrimary
        canvas.drawText("ODO: %.1f km".format(trip.odometerKm), cx, cy + s * 0.4f, pText)
    }
    
    private fun drawAnalogGauge(canvas: Canvas, gx: Float, gy: Float, r: Float, unit: String, value: Float, max: Float, warnColor: Int) {
        val start = 135f; val sweep = 270f
        rect.set(gx - r, gy - r, gx + r, gy + r)
        pStroke.strokeWidth = r * 0.1f; pStroke.color = theme.cardBg
        canvas.drawArc(rect, start, sweep, false, pStroke)
        val valAngle = (value / max).coerceIn(0f, 1f) * sweep
        pStroke.color = if (value > max * 0.85f) warnColor else theme.textPrimary
        canvas.drawArc(rect, start, valAngle, false, pStroke)
        pText.textSize = r * 0.2f; pText.textAlign = Paint.Align.CENTER; pText.color = theme.textSecondary
        canvas.drawText(unit, gx, gy + r * 0.4f, pText)
        pText.textSize = r * 0.3f; pText.color = theme.textPrimary
        canvas.drawText("%.0f".format(value), gx, gy + r * 0.1f, pText)
    }

    private fun drawGearLarge(canvas: Canvas, x: Float, y: Float, size: Float) {
        rect.set(x - size / 2, y - size / 2, x + size / 2, y + size / 2)
        pFill.color = theme.cardBg
        canvas.drawRoundRect(rect, 20f, 20f, pFill)
        pText.textSize = size * 0.8f
        pText.textAlign = Paint.Align.CENTER
        pText.color = if (data.gearPos == 0) theme.rpmGreen else theme.accent
        val g = if (data.gearPos == 0) "N" else "${data.gearPos}"
        canvas.drawText(g, x, y + size * 0.3f, pText)
    }

    private fun drawInfoPanel(canvas: Canvas, x: Float, y: Float, s: Float, label: String, value: String) {
        pText.textSize = s * 0.035f; pText.color = theme.textSecondary; pText.textAlign = Paint.Align.CENTER
        canvas.drawText(label, x, y, pText)
        pText.textSize = s * 0.055f; pText.color = theme.textPrimary
        canvas.drawText(value, x, y + s * 0.06f, pText)
    }

    private fun drawUniversalIndicators(canvas: Canvas, w: Float, h: Float, s: Float) {
        val bottomY = h - s * 0.15f
        val iconSize = s * 0.1f
        val icons = mutableListOf<Triple<String, Boolean, Int>>()
        icons.add(Triple("N", data.neutral, theme.rpmGreen))
        icons.add(Triple("OIL", false, theme.warningRed))
        icons.add(Triple("MIL", hasDtc, theme.warningRed))
        icons.add(Triple("HI", hiBeamOn, theme.accent2))
        icons.add(Triple("◄", leftTurnBlinking, theme.rpmGreen))
        icons.add(Triple("►", rightTurnBlinking, theme.rpmGreen))
        val totalWidth = icons.size * iconSize * 1.2f
        var startX = (w - totalWidth) / 2f
        icons.forEach { (label, active, color) ->
            val cx = startX + iconSize / 2f
            pFill.color = if (active) color else theme.cardBg
            canvas.drawCircle(cx, bottomY, iconSize * 0.45f, pFill)
            pText.textSize = iconSize * 0.4f; pText.textAlign = Paint.Align.CENTER
            pText.color = if (active) Color.BLACK else theme.textSecondary
            canvas.drawText(label, cx, bottomY + iconSize * 0.15f, pText)
            startX += iconSize * 1.2f
        }
    }
}
