package com.suzuki.ecutool.ui.dashboard

import android.os.Bundle
import android.view.*
import android.widget.Button
import android.widget.FrameLayout
import android.widget.PopupMenu
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import com.google.android.material.floatingactionbutton.FloatingActionButton
import com.google.android.material.materialswitch.MaterialSwitch
import com.suzuki.ecutool.R
import com.suzuki.ecutool.data.WidgetConfig
import com.suzuki.ecutool.service.ConnectionState
import com.suzuki.ecutool.ui.MainViewModel
import com.suzuki.ecutool.ui.dashboard.widgets.*

class DashboardEngineFragment : Fragment() {

    private val viewModel: MainViewModel by activityViewModels()
    private lateinit var manager: DashboardManager
    private lateinit var container: FrameLayout
    private lateinit var fabEdit: FloatingActionButton
    private lateinit var editControls: View
    private lateinit var swSnap: MaterialSwitch

    private val widgetViews = mutableListOf<BaseWidgetView>()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_dashboard_engine, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        manager = DashboardManager(requireContext())
        container = view.findViewById(R.id.dashContainer)
        fabEdit = view.findViewById(R.id.fabEdit)
        editControls = view.findViewById(R.id.editControls)
        swSnap = view.findViewById(R.id.swSnap)

        setupListeners(view)
        loadLayout()
        observeData()
    }

    private fun setupListeners(view: View) {
        fabEdit.setOnClickListener {
            manager.isEditMode = !manager.isEditMode
            updateEditModeUI()
        }

        swSnap.setOnCheckedChangeListener { _, checked ->
            manager.activeLayout.snapToGrid = checked
            manager.saveLayout()
            refreshWidgets()
        }

        view.findViewById<Button>(R.id.btnAddWidget).setOnClickListener {
            showAddWidgetMenu(it)
        }

        view.findViewById<Button>(R.id.btnSaveLoad).setOnClickListener {
            showLayoutSlotsDialog()
        }
        
        container.setOnLongClickListener {
            if (manager.isEditMode) showAddWidgetMenu(it)
            true
        }
    }

    private fun showLayoutSlotsDialog() {
        val popup = PopupMenu(requireContext(), editControls.findViewById(R.id.btnSaveLoad))
        
        popup.menu.add(0, 999, 0, "--- TEMPLATES ---")
        popup.menu.add(0, 100, 1, "Load Standard Template")
        popup.menu.add(0, 998, 2, "--- USER SLOTS ---")
        
        for (i in 0 until 10) {
            val file = manager.getSlotFile(i)
            val name = if (file.exists()) "Slot ${i+1} (Used)" else "Slot ${i+1} (Empty)"
            popup.menu.add(1, i, i + 10, name)
        }
        
        popup.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                100 -> {
                    manager.activeLayout = manager.createDefaultLayout(manager.activeSlot)
                    loadLayout()
                    Toast.makeText(context, "Loaded Standard Template", Toast.LENGTH_SHORT).show()
                }
                in 0..9 -> {
                    showSaveLoadOptions(item.itemId)
                }
            }
            true
        }
        popup.show()
    }

    private fun showSaveLoadOptions(slot: Int) {
        val dialog = androidx.appcompat.app.AlertDialog.Builder(requireContext())
            .setTitle("Layout Slot ${slot + 1}")
            .setMessage("Do you want to Save current or Load from this slot?")
            .setPositiveButton("SAVE") { _, _ ->
                manager.saveLayout(slot)
                Toast.makeText(context, "Saved to Slot ${slot + 1}", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("LOAD") { _, _ ->
                manager.activeLayout = manager.loadLayout(slot)
                loadLayout()
                Toast.makeText(context, "Loaded Slot ${slot + 1}", Toast.LENGTH_SHORT).show()
            }
            .setNeutralButton("Cancel", null)
            .show()
    }

    private fun updateEditModeUI() {
        editControls.visibility = if (manager.isEditMode) View.VISIBLE else View.GONE
        fabEdit.setImageResource(if (manager.isEditMode) android.R.drawable.ic_menu_save else android.R.drawable.ic_menu_edit)
        refreshWidgets()
    }

    private fun loadLayout() {
        container.removeAllViews()
        widgetViews.clear()
        
        manager.activeLayout.widgets.forEach { config ->
            createWidgetView(config)?.let {
                widgetViews.add(it)
                container.addView(it)
            }
        }
        swSnap.isChecked = manager.activeLayout.snapToGrid
    }

    private fun refreshWidgets() {
        widgetViews.forEach { it.updateLayoutFromConfig(); it.invalidate() }
    }

    private fun createWidgetView(config: WidgetConfig): BaseWidgetView? {
        val view = when (config.type) {
            "arc" -> ArcGaugeWidget(requireContext(), config, manager)
            "digital" -> DigitalWidget(requireContext(), config, manager)
            "bar" -> BarGaugeWidget(requireContext(), config, manager)
            "indicator" -> StatusIndicatorWidget(requireContext(), config, manager)
            else -> null
        }
        
        view?.setOnLongClickListener {
            if (manager.isEditMode) {
                showWidgetOptionsMenu(it as BaseWidgetView)
                true
            } else false
        }
        return view
    }

    private fun showWidgetOptionsMenu(widget: BaseWidgetView) {
        val popup = PopupMenu(requireContext(), widget)
        popup.menu.add("Delete")
        popup.setOnMenuItemClickListener {
            manager.removeWidget(widget.config.id)
            container.removeView(widget)
            widgetViews.remove(widget)
            true
        }
        popup.show()
    }

    private fun showAddWidgetMenu(anchor: View) {
        val popup = PopupMenu(requireContext(), anchor)
        popup.menu.add("Arc Tachometer")
        popup.menu.add("Digital Speedo")
        popup.menu.add("Gear Indicator")
        popup.menu.add("Coolant Bar")
        popup.menu.add("Neutral Light")
        
        popup.setOnMenuItemClickListener { item ->
            val id = "w_${System.currentTimeMillis()}"
            val newConfig = when (item.title) {
                "Arc Tachometer" -> WidgetConfig(
                    id = id, type = "arc", dataSource = "rpm",
                    gridW = 4, gridH = 4, max = 14000f, label = "RPM"
                )
                "Digital Speedo" -> WidgetConfig(
                    id = id, type = "digital", dataSource = "speed",
                    gridW = 3, gridH = 2, label = "km/h"
                )
                "Gear Indicator" -> WidgetConfig(
                    id = id, type = "digital", dataSource = "gear",
                    gridW = 2, gridH = 2, label = "GEAR"
                )
                "Coolant Bar" -> WidgetConfig(
                    id = id, type = "bar", dataSource = "coolant",
                    gridW = 1, gridH = 4, max = 120f, label = "COOL"
                )
                "Neutral Light" -> WidgetConfig(
                    id = id, type = "indicator", dataSource = "neutral",
                    gridW = 1, gridH = 1, label = "N"
                )
                else -> null
            }
            
            newConfig?.let {
                manager.addWidget(it)
                createWidgetView(it)?.let { view ->
                    widgetViews.add(view)
                    container.addView(view)
                }
            }
            true
        }
        popup.show()
    }

    private fun observeData() {
        viewModel.liveData.observe(viewLifecycleOwner) { data ->
            widgetViews.forEach { widget ->
                val value = when (widget.config.dataSource) {
                    "rpm" -> data.rpm.toFloat()
                    "speed" -> data.speedKmh()
                    "gear" -> data.gearPos.toFloat()
                    "coolant" -> data.coolantTemp.toFloat()
                    "bat" -> data.batteryVoltage()
                    "tps" -> data.throttlePos.toFloat()
                    "neutral" -> if (data.neutral) 1f else 0f
                    "clutch" -> if (data.clutchIn) 1f else 0f
                    else -> 0f
                }
                widget.onDataUpdate(value)
            }
        }

        viewModel.connectionState.observe(viewLifecycleOwner) { state ->
            view?.findViewById<View>(R.id.overlayFilter)?.visibility = 
                if (state is ConnectionState.Connected) View.GONE else View.VISIBLE
        }
    }
}
