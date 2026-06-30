package com.suzuki.ecutool.ui.dashboard.widgets

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.util.AttributeSet
import android.view.View

class RpmBarView(context: Context, attrs: AttributeSet? = null) : View(context, attrs) {
    var rpm: Int = 0
        set(value) {
            field = value
            invalidate()
        }

    private val bgPaint = Paint().apply { color = 0xFF1A1A1A.toInt() }
    private val barPaint = Paint()

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val ratio = (rpm.toFloat() / 15000f).coerceIn(0f, 1f)
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), bgPaint)
        
        barPaint.color = when {
            rpm > 12000 -> 0xFFFF0000.toInt()
            rpm > 10000 -> 0xFFFFFF00.toInt()
            else -> 0xFF00FF00.toInt()
        }
        
        canvas.drawRect(0f, 0f, width * ratio, height.toFloat(), barPaint)
    }
}
