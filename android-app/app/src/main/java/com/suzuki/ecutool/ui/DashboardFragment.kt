package com.suzuki.ecutool.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.suzuki.ecutool.R
import com.suzuki.ecutool.data.SDSData
import com.suzuki.ecutool.service.ConnectionState
import com.suzuki.ecutool.ui.dashboard.widgets.RpmBarView

class DashboardFragment : Fragment() {

    private val viewModel: MainViewModel by activityViewModels()
    private lateinit var rpmBar: RpmBarView
    private lateinit var speedoBig: TextView
    private lateinit var gearBig: TextView
    private lateinit var sensorGrid: RecyclerView
    private lateinit var connectOverlay: View
    private lateinit var btnConnect: Button
    private lateinit var statusLabel: TextView

    private val sensorAdapter = SensorAdapter()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_dashboard, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        rpmBar = view.findViewById(R.id.rpmBar)
        speedoBig = view.findViewById(R.id.speedoBig)
        gearBig = view.findViewById(R.id.gearBig)
        sensorGrid = view.findViewById(R.id.sensorGrid)
        connectOverlay = view.findViewById(R.id.connectOverlay)
        btnConnect = view.findViewById(R.id.btnConnectNow)
        statusLabel = view.findViewById(R.id.statusLabel)

        sensorGrid.layoutManager = GridLayoutManager(requireContext(), 2)
        sensorGrid.adapter = sensorAdapter

        btnConnect.setOnClickListener { viewModel.connect() }

        viewModel.liveData.observe(viewLifecycleOwner) { data ->
            updateUI(data)
        }

        viewModel.connectionState.observe(viewLifecycleOwner) { state ->
            when (state) {
                is ConnectionState.Connected -> {
                    connectOverlay.visibility = View.GONE
                }
                is ConnectionState.Connecting -> {
                    connectOverlay.visibility = View.VISIBLE
                    btnConnect.text = "CONNECTING..."
                    btnConnect.isEnabled = false
                    statusLabel.text = "Establishing K-Line link..."
                }
                else -> {
                    connectOverlay.visibility = View.VISIBLE
                    btnConnect.text = "CONNECT TO BIKE"
                    btnConnect.isEnabled = true
                    statusLabel.text = if (state is ConnectionState.Error) state.message else "Not connected to ECU"
                }
            }
        }
    }

    private fun updateUI(data: SDSData) {
        rpmBar.rpm = data.rpm
        speedoBig.text = data.speedKmh().toInt().toString()
        gearBig.text = if (data.gearPos == 0) "N" else data.gearPos.toString()
        
        val sensorList = listOf(
            SensorItem("COOLANT", "${data.coolantTemp} °C"),
            SensorItem("IAT", "${data.intakeAirTemp} °C"),
            SensorItem("BATTERY", "%.1f V".format(data.batteryVoltage())),
            SensorItem("TPS", "${data.throttlePos} %"),
            SensorItem("MAP", "${data.mapKpa} kPa"),
            SensorItem("BARO", "${data.baroKpa} kPa"),
            SensorItem("O2 SENSOR", "${data.o2Sensor} mV"),
            SensorItem("STPS", "${data.stps} %"),
            SensorItem("INJECTOR", "%.1f ms".format(data.injectorPulseMs())),
            SensorItem("IGNITION", "${data.ignitionTiming} °"),
            SensorItem("IAC STEP", "${data.iacStep}"),
            SensorItem("NEUTRAL", if (data.neutral) "YES" else "NO"),
            SensorItem("CLUTCH", if (data.clutchIn) "IN" else "OUT")
        )
        sensorAdapter.submitList(sensorList)
    }

    class SensorItem(val label: String, val value: String)

    class SensorAdapter : RecyclerView.Adapter<SensorAdapter.ViewHolder>() {
        private var items = emptyList<SensorItem>()
        fun submitList(list: List<SensorItem>) { items = list; notifyDataSetChanged() }
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = ViewHolder(
            LayoutInflater.from(parent.context).inflate(R.layout.item_sensor_card, parent, false)
        )
        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.label.text = items[position].label
            holder.value.text = items[position].value
        }
        override fun getItemCount() = items.size
        class ViewHolder(v: View) : RecyclerView.ViewHolder(v) {
            val label: TextView = v.findViewById(R.id.sensorLabel)
            val value: TextView = v.findViewById(R.id.sensorValue)
        }
    }

    fun setConfig(config: com.suzuki.ecutool.ui.DashConfig) {}
    fun setTheme(theme: Any) {}
    fun setLayout(layout: Any) {}
}
