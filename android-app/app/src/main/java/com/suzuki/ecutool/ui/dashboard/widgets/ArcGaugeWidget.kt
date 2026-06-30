package com.suzuki.ecutool.ui.dashboard.widgets

import android.content.Context
import android.graphics.*
import androidx.core.graphics.toColorInt
import com.suzuki.ecutool.data.WidgetConfig
import com.suzuki.ecutool.ui.dashboard.DashboardManager
import kotlin.math.min

class ArcGaugeWidget(
    context: Context,
    config: WidgetConfig,
    manager: DashboardManager
) : BaseWidgetView(context, config, manager) {

    private var currentVal = 0f
    private val rect = RectF()
    
    private val pStroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.BUTT
    }
    
    private val pText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD)
    }

    private val pBezel = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
    }

    override fun onDataUpdate(value: Float) {
        currentVal = currentVal * 0.7f + value * 0.3f
        postInvalidateOnAnimation()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        
        val w = width.toFloat(); val h = height.toFloat()
        val s = min(w, h); val cx = w/2f; val cy = h/2f
        val r = s * 0.42f
        
        // 1. Draw Outer 3D Shadow
        pBezel.shader = RadialGradient(cx, cy, r * 1.2f, 
            intArrayOf(Color.BLACK, Color.TRANSPARENT), 
            floatArrayOf(0.8f, 1f), Shader.TileMode.CLAMP)
        pBezel.style = Paint.Style.FILL
        canvas.drawCircle(cx, cy, r * 1.15f, pBezel)
        pBezel.shader = null

        // 2. Draw 3D Bezel (Metallic)
        pBezel.style = Paint.Style.STROKE
        pBezel.strokeWidth = s * 0.05f
        val bezelColors = intArrayOf("#222222".toColorInt(), "#777777".toColorInt(), "#333333".toColorInt(), "#111111".toColorInt())
        pBezel.shader = SweepGradient(cx, cy, bezelColors, null)
        canvas.drawCircle(cx, cy, r + pBezel.strokeWidth/2, pBezel)
        pBezel.shader = null

        // 3. Draw Background Arc
        rect.set(cx - r, cy - r, cx + r, cy + r)
        val startAngle = 140f; val sweepAngle = 260f
        val progress = (currentVal / config.max).coerceIn(0f, 1f)
        
        pStroke.strokeWidth = s * 0.08f
        pStroke.color = Color.BLACK
        pStroke.alpha = 180
        canvas.drawArc(rect, startAngle, sweepAngle, false, pStroke)
        
        // 4. Draw Ticks with Depth
        pStroke.strokeWidth = 3f
        pStroke.color = Color.WHITE
        pStroke.alpha = 100
        for (i in 0..10) {
            val angle = startAngle + (sweepAngle * (i / 10f))
            val rad = Math.toRadians(angle.toDouble())
            val innerR = r - s * 0.06f
            val x1 = cx + innerR * Math.cos(rad).toFloat()
            val y1 = cy + innerR * Math.sin(rad).toFloat()
            val x2 = cx + r * Math.cos(rad).toFloat()
            val y2 = cy + r * Math.sin(rad).toFloat()
            canvas.drawLine(x1, y1, x2, y2, pStroke)
        }

        // 5. Draw Progress Arc with Double Glow
        val theme = manager.activeLayout.themeName
        val accentColor = when(theme) {
            "Race" -> "#FF00FF".toColorInt()
            "Night" -> "#00B4FF".toColorInt()
            "Classic" -> "#C8AA64".toColorInt()
            else -> "#FF9800".toColorInt()
        }

        pStroke.strokeWidth = s * 0.07f
        pStroke.color = accentColor
        
        // Outer Glow
        pStroke.setShadowLayer(25f, 0f, 0f, accentColor)
        canvas.drawArc(rect, startAngle, sweepAngle * progress, false, pStroke)
        
        // Inner Core
        pStroke.strokeWidth = s * 0.03f
        pStroke.color = Color.WHITE
        pStroke.alpha = 200
        canvas.drawArc(rect, startAngle, sweepAngle * progress, false, pStroke)
        pStroke.clearShadowLayer()

        // 6. Digital Value in Center (LCD Style)
        pText.textSize = s * 0.22f
        pText.color = accentColor
        pText.alpha = 255
        pText.setShadowLayer(15f, 0f, 0f, accentColor)
        canvas.drawText("%.0f".format(currentVal), cx, cy + s * 0.08f, pText)
        pText.clearShadowLayer()
        
        config.label?.let {
            pText.textSize = s * 0.07f
            pText.color = Color.parseColor("#888888")
            canvas.drawText(it.uppercase(), cx, cy + s * 0.18f, pText)
        }

        // 7. Premium Glass Highlight (Curved)
        val glassPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.FILL
            shader = LinearGradient(0f, cy - r, 0f, cy, 
                intArrayOf(Color.WHITE, Color.TRANSPARENT),
                floatArrayOf(0f, 0.7f), Shader.TileMode.CLAMP)
            alpha = 40
        }
        canvas.drawCircle(cx, cy, r, glassPaint)
    }
}
