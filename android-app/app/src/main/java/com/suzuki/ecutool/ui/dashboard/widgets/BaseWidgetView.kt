package com.suzuki.ecutool.ui.dashboard.widgets

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.view.MotionEvent
import android.view.ViewGroup
import android.widget.FrameLayout
import com.suzuki.ecutool.data.WidgetConfig
import com.suzuki.ecutool.ui.dashboard.DashboardManager
import kotlin.math.max

abstract class BaseWidgetView(
    context: Context,
    val config: WidgetConfig,
    val manager: DashboardManager
) : FrameLayout(context) {

    private var lastX: Float = 0f
    private var lastY: Float = 0f
    private var isResizing: Boolean = false

    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
        color = Color.CYAN
        pathEffect = DashPathEffect(floatArrayOf(10f, 10f), 0f)
    }

    init {
        setWillNotDraw(false)
        updateLayoutFromConfig()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!manager.isEditMode) return false

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                lastX = event.rawX
                lastY = event.rawY
                isResizing = (event.x > width - 60 && event.y > height - 60)
                parent.requestDisallowInterceptTouchEvent(true)
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = event.rawX - lastX
                val dy = event.rawY - lastY

                if (isResizing) {
                    val newW = max(100, width + dx.toInt())
                    val newH = max(100, height + dy.toInt())
                    layoutParams.width = newW
                    layoutParams.height = newH
                } else {
                    x += dx
                    y += dy
                }

                lastX = event.rawX
                lastY = event.rawY
                requestLayout()
            }
            MotionEvent.ACTION_UP -> {
                saveFinalPosition()
            }
        }
        return true
    }

    private fun saveFinalPosition() {
        val parentView = parent as? ViewGroup ?: return
        val colWidth = parentView.width / 12f
        val rowHeight = parentView.height / 12f

        if (manager.activeLayout.snapToGrid) {
            config.gridX = (x / colWidth).toInt().coerceIn(0, 11)
            config.gridY = (y / rowHeight).toInt().coerceIn(0, 11)
            config.gridW = (width / colWidth).toInt().coerceAtLeast(1)
            config.gridH = (height / rowHeight).toInt().coerceAtLeast(1)
            updateLayoutFromConfig()
        } else {
            config.posX = x
            config.posY = y
            config.gridW = width
            config.gridH = height
        }
        manager.saveLayout()
    }

    fun updateLayoutFromConfig() {
        post {
            val parentView = parent as? ViewGroup ?: return@post
            val colWidth = parentView.width / 12f
            val rowHeight = parentView.height / 12f

            val params = layoutParams ?: ViewGroup.LayoutParams(0, 0)
            if (manager.activeLayout.snapToGrid) {
                params.width = (config.gridW * colWidth).toInt()
                params.height = (config.gridH * rowHeight).toInt()
                x = config.gridX * colWidth
                y = config.gridY * rowHeight
            } else {
                params.width = config.gridW
                params.height = config.gridH
                x = config.posX
                y = config.posY
            }
            layoutParams = params
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (manager.isEditMode) {
            canvas.drawRect(2f, 2f, width.toFloat() - 2f, height.toFloat() - 2f, borderPaint)
            val path = android.graphics.Path().apply {
                moveTo(width.toFloat(), height.toFloat() - 40f)
                lineTo(width.toFloat(), height.toFloat())
                lineTo(width.toFloat() - 40f, height.toFloat())
                close()
            }
            borderPaint.style = Paint.Style.FILL
            canvas.drawPath(path, borderPaint)
            borderPaint.style = Paint.Style.STROKE
        }
    }
    
    abstract fun onDataUpdate(value: Float)
}
