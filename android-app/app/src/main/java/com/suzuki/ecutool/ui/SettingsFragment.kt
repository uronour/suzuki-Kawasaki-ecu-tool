package com.suzuki.ecutool.ui

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.Button
import android.widget.EditText
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.materialswitch.MaterialSwitch
import com.suzuki.ecutool.R
import com.suzuki.ecutool.service.ConnectionState

class SettingsFragment : Fragment() {

    private val viewModel: MainViewModel by activityViewModels()
    private lateinit var hostInput: EditText
    private lateinit var statusText: TextView
    private lateinit var btnConnect: Button
    private lateinit var btnDisconnect: Button
    private lateinit var btnScanBT: Button
    private lateinit var rgBrand: android.widget.RadioGroup
    private lateinit var swAutoConnect: MaterialSwitch
    private lateinit var spinnerPoll: Spinner
    private lateinit var swKeepScreenOn: MaterialSwitch
    private lateinit var swUnitKmh: MaterialSwitch

    companion object {
        private const val TARGET_ECU_MAC = "98:D3:34:90:C9:D9"
        private const val TARGET_ECU_NAME = "SuzukiECU"
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_settings, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        try {
            setupViews(view)
        } catch (e: Exception) {
            android.util.Log.e("SettingsFragment", "Setup failed", e)
        }
    }

    private fun setupViews(view: View) {
        hostInput = view.findViewById(R.id.hostInput)
        statusText = view.findViewById(R.id.settingsStatus)
        btnConnect = view.findViewById(R.id.btnConnect)
        btnDisconnect = view.findViewById(R.id.btnDisconnect)
        btnScanBT = view.findViewById(R.id.btnScanBT)
        rgBrand = view.findViewById(R.id.rgBrand)
        swAutoConnect = view.findViewById(R.id.swAutoConnect)
        spinnerPoll = view.findViewById(R.id.spinnerPollInterval)
        swKeepScreenOn = view.findViewById(R.id.swKeepScreenOn)
        swUnitKmh = view.findViewById(R.id.swUnitKmh)

        val ctx = requireContext()
        val prefs = ctx.getSharedPreferences("app_settings", 0)

        hostInput.setText(prefs.getString("last_bt_address", ""))
        hostInput.hint = "Bluetooth Address"

        swAutoConnect.isChecked = prefs.getBoolean("auto_connect", false)
        swKeepScreenOn.isChecked = prefs.getBoolean("keep_screen_on", true)

        val cfg = DashConfigManager(ctx)
        swUnitKmh.isChecked = cfg.load().unitKmh

        view.findViewById<TextView>(R.id.buildVersion)?.text = "BUILD: ${MainActivity.BUILD_VERSION}"

        val pollValues = intArrayOf(100, 500, 1000, 2000, 5000)
        val savedPoll = prefs.getInt("poll_interval", 1000)
        val pollIdx = pollValues.indexOfFirst { it == savedPoll }.coerceAtLeast(0)
        spinnerPoll.setSelection(pollIdx)

        btnConnect.setOnClickListener {
            val address = hostInput.text.toString().trim()
            if (address.isNotEmpty()) {
                prefs.edit().putString("last_bt_address", address).apply()
                viewModel.disconnect()
                viewModel.connect(address)
            }
        }

        btnDisconnect.setOnClickListener { viewModel.disconnect() }

        btnScanBT.setOnClickListener { showBtScanDialog() }

        rgBrand.check(if (viewModel.ecuBrand.value == 1) R.id.rbKawasaki else R.id.rbSuzuki)
        rgBrand.setOnCheckedChangeListener { _, id ->
            viewModel.setBrand(if (id == R.id.rbKawasaki) 1 else 0)
        }
        
        viewModel.connectionState.observe(viewLifecycleOwner) { state ->
            val connected = state is ConnectionState.Connected
            btnConnect.isEnabled = !connected
            btnDisconnect.isEnabled = connected
            statusText.text = when (state) {
                is ConnectionState.Connected -> "BT Connected: ${state.host}"
                is ConnectionState.Connecting -> "Connecting BT..."
                is ConnectionState.Disconnected -> state.reason?.let { "Disconnected: $it" } ?: "Disconnected"
                is ConnectionState.Error -> "BT Error: ${state.message}"
            }
        }

        swAutoConnect.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean("auto_connect", checked).apply()
        }

        spinnerPoll.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                prefs.edit().putInt("poll_interval", pollValues[pos]).apply()
                viewModel.updatePollInterval(pollValues[pos])
            }
            override fun onNothingSelected(p: AdapterView<*>?) {}
        }

        swKeepScreenOn.setOnCheckedChangeListener { _, checked ->
            prefs.edit().putBoolean("keep_screen_on", checked).apply()
            requireActivity().window?.let { win ->
                if (checked) win.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                else win.clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            }
        }

        swUnitKmh.setOnCheckedChangeListener { _, checked ->
            val c = cfg.load().copy(unitKmh = checked)
            cfg.save(c)
            (requireActivity() as? MainActivity)?.applyDashConfig(c)
        }

        view.findViewById<Button>(R.id.themeSport)?.setOnClickListener { applyThemeSafe("Sport") }
        view.findViewById<Button>(R.id.themeClassic)?.setOnClickListener { applyThemeSafe("Classic") }
        view.findViewById<Button>(R.id.themeNight)?.setOnClickListener { applyThemeSafe("Night") }
        view.findViewById<Button>(R.id.themeRace)?.setOnClickListener { applyThemeSafe("Race") }
        
        view.findViewById<Button>(R.id.layoutSport)?.setOnClickListener { applyLayoutSafe("Sport") }
        view.findViewById<Button>(R.id.layoutRace)?.setOnClickListener { applyLayoutSafe("Race") }
        view.findViewById<Button>(R.id.layoutClassic)?.setOnClickListener { applyLayoutSafe("Classic") }

        view.findViewById<Button>(R.id.presetCyber)?.setOnClickListener { viewModel.triggerPreset("Cyberpunk") }
        view.findViewById<Button>(R.id.presetFuturistic)?.setOnClickListener { viewModel.triggerPreset("Futuristic") }
        view.findViewById<Button>(R.id.presetPipBoy)?.setOnClickListener { viewModel.triggerPreset("Pip-Boy") }

        setupToggle(view, R.id.swRpm, "rpm")
        setupToggle(view, R.id.swSpeedo, "speedo")
        setupToggle(view, R.id.swGear, "gear")
        setupToggle(view, R.id.swClock, "clock")
        setupToggle(view, R.id.swOdo, "odo")
        setupToggle(view, R.id.swFuel, "fuel")
        setupToggle(view, R.id.swPanel, "panel")
        setupToggle(view, R.id.swNeutral, "neutral")
        setupToggle(view, R.id.swFan, "fan")
        setupToggle(view, R.id.swClutch, "clutch")
        setupToggle(view, R.id.swSidestand, "sidestand")
        setupToggle(view, R.id.swCheckEng, "check_eng")
        setupToggle(view, R.id.swOil, "oil")
        setupToggle(view, R.id.swHiBeam, "hibeam")
        setupToggle(view, R.id.swTurns, "turns")
        setupToggle(view, R.id.swLargeKm, "km_large")

        if (swKeepScreenOn.isChecked) {
            requireActivity().window?.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
    }

    private fun applyThemeSafe(name: String) {
        try {
            (requireActivity() as? MainActivity)?.applyGaugeTheme(name)
            statusText.text = "$name theme"
        } catch (_: Exception) {}
    }

    private fun applyLayoutSafe(name: String) {
        try {
            (requireActivity() as? MainActivity)?.applyDashLayout(name)
            statusText.text = "$name layout"
        } catch (_: Exception) {}
    }

    private fun setupToggle(view: View, id: Int, key: String) {
        val sw = view.findViewById<MaterialSwitch>(id) ?: return
        val cfg = DashConfigManager(requireContext())
        val current = cfg.load()
        sw.isChecked = when (key) {
            "rpm" -> current.showRpmGauge; "speedo" -> current.showSpeedo
            "gear" -> current.showGear; "clock" -> current.showClock
            "odo" -> current.showOdometer; "fuel" -> current.showFuelGauge
            "panel" -> current.showCenterPanel; "neutral" -> current.showNeutral
            "fan" -> current.showFan; "clutch" -> current.showClutch
            "sidestand" -> current.showSidestand; "check_eng" -> current.showCheckEngine
            "oil" -> current.showOilLight; "hibeam" -> current.showHiBeam
            "turns" -> current.showTurnSignals; "km_large" -> current.showKmLarge
            else -> true
        }
        sw.setOnCheckedChangeListener { _, checked ->
            try {
                val c = cfg.load()
                val updated = when (key) {
                    "rpm" -> c.copy(showRpmGauge = checked); "speedo" -> c.copy(showSpeedo = checked)
                    "gear" -> c.copy(showGear = checked); "clock" -> c.copy(showClock = checked)
                    "odo" -> c.copy(showOdometer = checked); "fuel" -> c.copy(showFuelGauge = checked)
                    "panel" -> c.copy(showCenterPanel = checked); "neutral" -> c.copy(showNeutral = checked)
                    "fan" -> c.copy(showFan = checked); "clutch" -> c.copy(showClutch = checked)
                    "sidestand" -> c.copy(showSidestand = checked); "check_eng" -> c.copy(showCheckEngine = checked)
                    "oil" -> c.copy(showOilLight = checked); "hibeam" -> c.copy(showHiBeam = checked)
                    "turns" -> c.copy(showTurnSignals = checked); "km_large" -> c.copy(showKmLarge = checked)
                    else -> c
                }
                (requireActivity() as? MainActivity)?.applyDashConfig(updated)
            } catch (_: Exception) {}
        }
    }

    private fun showBtScanDialog() {
        try {
            val bluetoothManager = requireContext().getSystemService(Context.BLUETOOTH_SERVICE) as android.bluetooth.BluetoothManager
            val adapter = bluetoothManager.adapter
            if (adapter == null) {
                Toast.makeText(requireContext(), "Bluetooth not supported on this device", Toast.LENGTH_SHORT).show()
                return
            }
            
            // 1. Check if the target ECU is already bonded
            val bondedDevices = try { adapter.bondedDevices.toList() } catch (e: SecurityException) { emptyList() }
            val targetDevice = bondedDevices.find { it.address.equals(TARGET_ECU_MAC, ignoreCase = true) }
            
            if (targetDevice != null) {
                Toast.makeText(requireContext(), "Target ECU found! Connecting...", Toast.LENGTH_SHORT).show()
                hostInput.setText(targetDevice.address)
                viewModel.connect(targetDevice.address)
                return
            }

            // 2. Filter list if target not found immediately
            val filteredDevices = bondedDevices.filter { 
                it.name?.contains("ECU", ignoreCase = true) == true || 
                it.name?.contains("Suzuki", ignoreCase = true) == true ||
                it.name?.contains("Kawasaki", ignoreCase = true) == true
            }

            // If we have filters, use them, otherwise show all
            val finalDeviceList = if (filteredDevices.isNotEmpty()) filteredDevices else bondedDevices

            val dialog = androidx.appcompat.app.AlertDialog.Builder(requireContext())
            val dialogView = layoutInflater.inflate(R.layout.dialog_bt_scan, null)
            dialog.setView(dialogView)
            
            val recyclerView = dialogView.findViewById<RecyclerView>(R.id.btDeviceList)
            val btnRefresh = dialogView.findViewById<Button>(R.id.btnRefresh)
            
            val currentDialog = dialog.create()
            
            val btAdapter = BTDeviceAdapter(finalDeviceList) { device ->
                hostInput.setText(device.address)
                currentDialog.dismiss()
                viewModel.connect(device.address)
            }
            
            recyclerView.layoutManager = LinearLayoutManager(requireContext())
            recyclerView.adapter = btAdapter
            
            btnRefresh.setOnClickListener {
                try {
                    val latestBonded = adapter.bondedDevices.toList()
                    btAdapter.updateList(latestBonded)
                } catch (e: SecurityException) {
                    Toast.makeText(requireContext(), "Permission error", Toast.LENGTH_SHORT).show()
                }
            }
            
            currentDialog.show()
        } catch (e: Exception) {
            android.util.Log.e("SettingsFragment", "BT Dialog failed", e)
            Toast.makeText(requireContext(), "BT Error: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }
}

class BTDeviceAdapter(private var items: List<android.bluetooth.BluetoothDevice>, private val onClick: (android.bluetooth.BluetoothDevice) -> Unit) : 
    RecyclerView.Adapter<BTDeviceAdapter.ViewHolder>() {

    fun updateList(newList: List<android.bluetooth.BluetoothDevice>) {
        items = newList
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.bt_device_item, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val device = items[position]
        holder.name.text = device.name ?: "Unknown"
        holder.address.text = device.address
        holder.itemView.setOnClickListener { onClick(device) }
    }

    override fun getItemCount() = items.size

    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val name: TextView = view.findViewById(R.id.deviceName)
        val address: TextView = view.findViewById(R.id.deviceAddress)
    }
}
