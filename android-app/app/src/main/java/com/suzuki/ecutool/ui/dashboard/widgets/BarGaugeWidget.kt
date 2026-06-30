package com.suzuki.ecutool.ui.dashboard.widgets

import android.content.Context
import android.graphics.*
import androidx.core.graphics.toColorInt
import com.suzuki.ecutool.data.WidgetConfig
import com.suzuki.ecutool.ui.dashboard.DashboardManager

class BarGaugeWidget(
    context: Context,
    config: WidgetConfig,
    manager: DashboardManager
) : BaseWidgetView(context, config, manager) {

    private var currentVal = 0f
    private val pFill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val pText = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }

    override fun onDataUpdate(value: Float) {
        currentVal = currentVal * 0.8f + value * 0.2f
        postInvalidateOnAnimation()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat(); val h = height.toFloat()
        
        // 1. Container Background (Deep Dark)
        pFill.color = "#050505".toColorInt()
        canvas.drawRoundRect(0f, 0f, w, h, 8f, 8f, pFill)
        
        // 2. Bezel Highlight
        pFill.style = Paint.Style.STROKE
        pFill.strokeWidth = 2f
        pFill.color = "#222222".toColorInt()
        canvas.drawRoundRect(1f, 1f, w-1f, h-1f, 7f, 7f, pFill)
        pFill.style = Paint.Style.FILL

        // 3. Theme Colors
        val theme = manager.activeLayout.themeName
        val activeColor = when(theme) {
            "Race" -> "#FF00FF".toColorInt()
            "Night" -> "#00B4FF".toColorInt()
            "Classic" -> "#00FF00".toColorInt()
            else -> "#FF9800".toColorInt()
        }

        // 4. Draw Segments (LED Style)
        val margin = 10f
        val segCount = 15
        val segGap = 4f
        val segAreaH = h - (margin * 4)
        val segH = (segAreaH - (segCount * segGap)) / segCount
        
        val progress = (currentVal / config.max).coerceIn(0f, 1f)
        val filledCount = (progress * segCount).toInt()

        for (i in 0 until segCount) {
            val isFilled = i < filledCount
            pFill.color = if (isFilled) activeColor else "#151515".toColorInt()
            if (isFilled) pFill.setShadowLayer(10f, 0f, 0f, activeColor)
            
            val bottom = h - margin - (i * (segH + segGap))
            val top = bottom - segH
            canvas.drawRect(margin, top, w - margin, bottom, pFill)
            pFill.clearShadowLayer()
        }
        
        // 5. Centered Digital Value
        pText.textSize = h * 0.15f
        pText.color = Color.WHITE
        pText.typeface = Typeface.DEFAULT_BOLD
        canvas.drawText("%.0f".format(currentVal), w/2f, margin * 2.5f, pText)
    }
}
