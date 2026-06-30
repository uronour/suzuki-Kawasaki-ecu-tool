package com.suzuki.ecutool.service

import android.app.Service
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import android.util.Log
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID

class BluetoothService : Service() {

    private val binder = LocalBinder()
    private val serviceScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var socket: BluetoothSocket? = null
    private var readerJob: Job? = null
    private val sendMutex = Mutex()
    
    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Disconnected())
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _responseFlow = MutableStateFlow("")
    val responseFlow: StateFlow<String> = _responseFlow.asStateFlow()

    private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    inner class LocalBinder : Binder() {
        fun getService(): BluetoothService = this@BluetoothService
    }

    override fun onBind(intent: Intent?): IBinder = binder

    fun connect(address: String) {
        if (_connectionState.value is ConnectionState.Connecting) return
        
        disconnect()

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = bluetoothManager.adapter
        val device = try { adapter.getRemoteDevice(address) } catch (e: Exception) {
            _connectionState.value = ConnectionState.Error("Invalid address")
            return
        }

        _connectionState.value = ConnectionState.Connecting
        serviceScope.launch {
            try {
                Log.d(TAG, "Connecting to BT device: $address")
                socket = device.createRfcommSocketToServiceRecord(SPP_UUID)
                socket?.connect()

                _connectionState.value = ConnectionState.Connected(address, 0)
                startReader()
            } catch (e: Exception) {
                Log.e(TAG, "BT connection failed", e)
                _connectionState.value = ConnectionState.Error("BT failed: ${e.message}")
                cleanup()
            }
        }
    }

    fun disconnect() {
        serviceScope.launch {
            cleanup()
            _connectionState.value = ConnectionState.Disconnected()
        }
    }

    fun sendCommand(cmd: String) {
        val s = socket ?: return
        if (!s.isConnected) return
        
        serviceScope.launch {
            sendMutex.withLock {
                try {
                    val out: OutputStream = s.outputStream
                    out.write((cmd + "\r\n").toByteArray(Charsets.US_ASCII))
                    out.flush()
                    Log.d(TAG, "Sent BT: $cmd")
                } catch (e: Exception) {
                    Log.e(TAG, "BT Send failed", e)
                }
            }
        }
    }

    private fun startReader() {
        readerJob?.cancel()
        readerJob = serviceScope.launch {
            try {
                val input: InputStream = socket?.inputStream ?: return@launch
                val buffer = ByteArray(1024)
                val lineBuffer = StringBuilder()

                while (isActive && socket?.isConnected == true) {
                    val bytes = input.read(buffer)
                    if (bytes > 0) {
                        val text = String(buffer, 0, bytes, Charsets.US_ASCII)
                        for (c in text) {
                            if (c == '\r' || c == '\n') {
                                if (lineBuffer.isNotEmpty()) {
                                    val line = lineBuffer.toString().trim()
                                    if (line.isNotEmpty()) {
                                        Log.d(TAG, "Received BT: $line")
                                        _responseFlow.value = line
                                    }
                                    lineBuffer.clear()
                                }
                            } else {
                                lineBuffer.append(c)
                            }
                        }
                    } else if (bytes == -1) {
                        break
                    }
                }
            } catch (e: Exception) {
                if (isActive) {
                    Log.e(TAG, "BT Reader error", e)
                }
            } finally {
                _connectionState.value = ConnectionState.Disconnected("BT connection lost")
                cleanup()
            }
        }
    }

    private fun cleanup() {
        try { socket?.close() } catch (_: Exception) {}
        socket = null
        readerJob?.cancel()
    }

    override fun onDestroy() {
        cleanup()
        serviceScope.cancel()
        super.onDestroy()
    }

    companion object {
        private const val TAG = "BluetoothService"
    }
}