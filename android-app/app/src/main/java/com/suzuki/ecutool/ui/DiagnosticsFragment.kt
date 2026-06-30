package com.suzuki.ecutool.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.suzuki.ecutool.R
import com.suzuki.ecutool.data.DTC

class DiagnosticsFragment : Fragment() {

    private val viewModel: MainViewModel by activityViewModels()
    private lateinit var dtcList: RecyclerView
    private lateinit var btnReadDTC: Button
    private lateinit var btnLoopbackTest: Button
    private lateinit var dtcCountText: TextView
    private lateinit var loopbackResultText: TextView
    private var dtcAdapter: DTCAdapter? = null

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_diagnostics, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        dtcList = view.findViewById(R.id.dtcList)
        btnReadDTC = view.findViewById(R.id.btnReadDTC)
        btnLoopbackTest = view.findViewById(R.id.btnLoopbackTest)
        dtcCountText = view.findViewById(R.id.dtcCountText)
        loopbackResultText = view.findViewById(R.id.loopbackResultText)

        dtcAdapter = DTCAdapter()
        dtcList.layoutManager = LinearLayoutManager(requireContext())
        dtcList.adapter = dtcAdapter

        btnReadDTC.setOnClickListener {
            viewModel.requestDTC()
        }

        viewModel.dtcList.observe(viewLifecycleOwner) { dtcs ->
            dtcAdapter?.submitList(dtcs)
            dtcCountText.text = getString(R.string.dtc_count_format, dtcs.size)
        }

        viewModel.statusMessage.observe(viewLifecycleOwner) { msg ->
            if (msg.isNotEmpty() && msg.contains("DTC")) {
                dtcCountText.text = msg
            }
        }

        view.findViewById<Button>(R.id.btnClearDTC)?.setOnClickListener {
            viewModel.sendCommand("clear_dtc")
        }

        btnLoopbackTest.setOnClickListener {
            viewModel.runLoopbackTest("KLINE_LOOPBACK_" + System.currentTimeMillis() % 1000)
        }

        view.findViewById<Button>(R.id.btnPing)?.setOnClickListener {
            viewModel.sendPing()
        }

        viewModel.loopbackResult.observe(viewLifecycleOwner) { result ->
            loopbackResultText.text = "Loopback Result: $result"
        }

        viewModel.isLoopbackRunning.observe(viewLifecycleOwner) { running ->
            btnLoopbackTest.isEnabled = !running
            if (running) {
                loopbackResultText.text = "Loopback Test: Running..."
            }
        }
    }
}

class DTCAdapter : RecyclerView.Adapter<DTCAdapter.ViewHolder>() {

    private var items: List<DTC> = emptyList()

    fun submitList(list: List<DTC>) {
        items = list
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(android.R.layout.simple_list_item_2, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val dtc = items[position]
        holder.text1.text = com.suzuki.ecutool.util.DTCUtils.getDisplayCode(dtc.code)
        holder.text2.text = com.suzuki.ecutool.util.DTCUtils.getDescription(dtc.code)
    }

    override fun getItemCount(): Int = items.size

    class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val text1: android.widget.TextView = view.findViewById(android.R.id.text1)
        val text2: android.widget.TextView = view.findViewById(android.R.id.text2)
    }
}
