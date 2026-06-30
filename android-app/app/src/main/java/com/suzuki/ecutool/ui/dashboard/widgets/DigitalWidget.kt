package com.suzuki.ecutool.ui.dashboard.widgets

import android.content.Context
import android.graphics.*
import androidx.core.graphics.toColorInt
import com.suzuki.ecutool.data.WidgetConfig
import com.suzuki.ecutool.ui.dashboard.DashboardManager

class DigitalWidget(
    context: Context,
    config: WidgetConfig,
    manager: DashboardManager
) : BaseWidgetView(context, config, manager) {

    private var currentVal = 0f
    private val pText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        typeface = Typeface.MONOSPACE
    }
    
    private val pBg = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }

    override fun onDataUpdate(value: Float) {
        currentVal = value
        postInvalidateOnAnimation()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        
        val w = width.toFloat(); val h = height.toFloat()
        val cx = w/2f; val cy = h/2f
        
        // 1. 3D Recessed Frame
        pBg.color = "#080808".toColorInt()
        canvas.drawRoundRect(0f, 0f, w, h, 12f, 12f, pBg)
        
        pBg.style = Paint.Style.STROKE
        pBg.strokeWidth = 3f
        pBg.color = "#222222".toColorInt()
        canvas.drawRoundRect(2f, 2f, w-2f, h-2f, 10f, 10f, pBg)
        
        // Internal shadow
        pBg.style = Paint.Style.FILL
        pBg.shader = LinearGradient(0f, 0f, 0f, 20f, 
            intArrayOf(Color.BLACK, Color.TRANSPARENT), null, Shader.TileMode.CLAMP)
        canvas.drawRoundRect(4f, 4f, w-4f, h-4f, 8f, 8f, pBg)
        pBg.shader = null

        // 2. LCD Colors
        val theme = manager.activeLayout.themeName
        val activeColor = when(theme) {
            "Race" -> "#FF00FF".toColorInt()
            "Night" -> "#00B4FF".toColorInt()
            "Classic" -> "#00FF00".toColorInt()
            else -> "#FF9800".toColorInt()
        }

        // 3. Ghost Digits (LCD effect)
        pText.textSize = h * 0.7f
        pText.color = activeColor
        pText.alpha = 15
        canvas.drawText("888", cx, cy + h * 0.25f, pText)
        
        // 4. Active Digits with Heavy Glow
        pText.alpha = 255
        pText.setShadowLayer(15f, 0f, 0f, activeColor)
        val displayStr = if (config.dataSource == "gear" && currentVal == 0f) "N" else "%.0f".format(currentVal)
        canvas.drawText(displayStr, cx, cy + h * 0.25f, pText)
        pText.clearShadowLayer()
        
        // 5. Label with modern styling
        config.label?.let {
            pText.textSize = h * 0.12f
            pText.color = "#666666".toColorInt()
            pText.typeface = Typeface.DEFAULT_BOLD
            canvas.drawText(it.uppercase(), cx, cy + h * 0.45f, pText)
        }
    }
}
