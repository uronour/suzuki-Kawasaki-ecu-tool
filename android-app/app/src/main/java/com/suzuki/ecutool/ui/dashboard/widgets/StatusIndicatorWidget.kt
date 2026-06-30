package com.suzuki.ecutool.ui.dashboard.widgets

import android.content.Context
import android.graphics.*
import androidx.core.graphics.toColorInt
import com.suzuki.ecutool.data.WidgetConfig
import com.suzuki.ecutool.ui.dashboard.DashboardManager

class StatusIndicatorWidget(
    context: Context,
    config: WidgetConfig,
    manager: DashboardManager
) : BaseWidgetView(context, config, manager) {

    private var isActive = false
    private val pFill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }

    override fun onDataUpdate(value: Float) {
        isActive = value > 0.5f
        postInvalidateOnAnimation()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat(); val h = height.toFloat()
        val s = minOf(w, h)
        val cx = w/2f; val cy = h/2f
        
        val color = when(config.dataSource) {
            "neutral" -> "#00FF00".toColorInt()
            "clutch", "fan" -> "#00B4FF".toColorInt()
            "sidestand", "check_eng", "oil" -> "#FF0000".toColorInt()
            else -> "#FF9800".toColorInt()
        }

        // 1. Draw 3D Bezel
        pFill.color = "#222222".toColorInt()
        canvas.drawCircle(cx, cy, s * 0.45f, pFill)
        
        // 2. Draw Lens
        val lensColor = if (isActive) color else "#151515".toColorInt()
        pFill.color = lensColor
        if (isActive) pFill.setShadowLayer(15f, 0f, 0f, color)
        canvas.drawCircle(cx, cy, s * 0.38f, pFill)
        pFill.clearShadowLayer()

        // 3. Lens Highlight
        pFill.shader = RadialGradient(cx - s*0.1f, cy - s*0.1f, s*0.3f, 
            intArrayOf(Color.WHITE, Color.TRANSPARENT), null, Shader.TileMode.CLAMP)
        pFill.alpha = if (isActive) 100 else 20
        canvas.drawCircle(cx, cy, s * 0.38f, pFill)
        pFill.shader = null

        // 4. Label
        val pText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            textAlign = Paint.Align.CENTER
            textSize = s * 0.25f
            typeface = Typeface.DEFAULT_BOLD
            this.color = if (isActive) Color.WHITE else "#444444".toColorInt()
        }
        canvas.drawText(config.label?.uppercase() ?: "", cx, cy + s * 0.08f, pText)
    }
}
