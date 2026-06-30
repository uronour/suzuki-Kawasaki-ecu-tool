package com.suzuki.ecutool.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.ScrollView
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import com.suzuki.ecutool.R

class TerminalFragment : Fragment() {

    private val viewModel: MainViewModel by activityViewModels()
    
    private lateinit var terminalInput: EditText
    private lateinit var terminalSend: Button
    private lateinit var terminalClear: Button
    private lateinit var terminalPause: Button
    private lateinit var terminalHistory: TextView
    private lateinit var terminalScroll: ScrollView

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_terminal, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        terminalInput = view.findViewById(R.id.terminalInput)
        terminalSend = view.findViewById(R.id.terminalSend)
        terminalClear = view.findViewById(R.id.terminalClear)
        terminalPause = view.findViewById(R.id.terminalPause)
        terminalHistory = view.findViewById(R.id.terminalHistory)
        terminalScroll = view.findViewById(R.id.terminalScroll)

        setupListeners()
        setupObservers()
    }

    private fun setupListeners() {
        terminalSend.setOnClickListener {
            val cmd = terminalInput.text.toString().trim()
            if (cmd.isNotEmpty()) {
                viewModel.sendCommand(cmd)
                terminalInput.text.clear()
            }
        }

        terminalClear.setOnClickListener {
            viewModel.clearTerminal()
        }

        terminalPause.setOnClickListener {
            val paused = viewModel.togglePolling()
            terminalPause.text = if (paused) "RESUME" else "PAUSE"
            terminalPause.setTextColor(if (paused) 0xFFFF0000.toInt() else 0xFFB0B0B0.toInt())
        }
    }

    private fun setupObservers() {
        viewModel.terminalLog.observe(viewLifecycleOwner) { log ->
            terminalHistory.text = log.joinToString("\n")
            terminalScroll.post { terminalScroll.fullScroll(View.FOCUS_DOWN) }
        }
    }
}
