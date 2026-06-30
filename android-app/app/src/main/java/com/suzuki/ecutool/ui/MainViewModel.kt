package com.suzuki.ecutool.ui

import android.app.Application
import android.bluetooth.BluetoothManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import com.suzuki.ecutool.data.DTC
import com.suzuki.ecutool.data.SDSData
import com.suzuki.ecutool.data.SDSEcuInfo
import com.suzuki.ecutool.service.ConnectionState
import com.suzuki.ecutool.service.BluetoothService
import com.suzuki.ecutool.util.ParsedResponse
import com.suzuki.ecutool.util.SDSProtocol
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File

class MainViewModel(application: Application) : AndroidViewModel(application) {

    private var btService: BluetoothService? = null
    
    private var streamJob: Job? = null
    private var pollJob: Job? = null
    private var pollIntervalMs: Long = 100
    private var isPollingPaused = false

    private val _connectionState = MutableLiveData<ConnectionState>(ConnectionState.Disconnected())
    val connectionState: LiveData<ConnectionState> = _connectionState

    private val _liveData = MutableLiveData(SDSData())
    val liveData: LiveData<SDSData> = _liveData

    private val _ecuInfo = MutableLiveData(SDSEcuInfo())
    val ecuInfo: LiveData<SDSEcuInfo> = _ecuInfo

    private val _dtcList = MutableLiveData<List<DTC>>(emptyList())
    val dtcList: LiveData<List<DTC>> = _dtcList

    private val _statusMessage = MutableLiveData("")
    val statusMessage: LiveData<String> = _statusMessage

    private val _isStreaming = MutableLiveData(false)
    val isStreaming: LiveData<Boolean> = _isStreaming

    private val _presetSignal = MutableLiveData<String>()
    val presetSignal: LiveData<String> = _presetSignal

    private val _loopbackResult = MutableLiveData<String>()
    val loopbackResult: LiveData<String> = _loopbackResult

    private val _terminalLog = MutableLiveData<List<String>>(emptyList())
    val terminalLog: LiveData<List<String>> = _terminalLog

    private val _isLoopbackRunning = MutableLiveData(false)
    val isLoopbackRunning: LiveData<Boolean> = _isLoopbackRunning

    private val _updateInfo = MutableLiveData<com.suzuki.ecutool.util.UpdateInfo?>(null)
    val updateInfo: LiveData<com.suzuki.ecutool.util.UpdateInfo?> = _updateInfo

    private val _updateReady = MutableLiveData<File?>(null)
    val updateReady: LiveData<File?> = _updateReady

    private val _ecuBrand = MutableLiveData(0) // 0=Suzuki, 1=Kawasaki
    val ecuBrand: LiveData<Int> = _ecuBrand

    companion object {
        private const val TAG = "MainViewModel"
        private const val TARGET_ECU_MAC = "98:D3:34:90:C9:D9"
    }

    private val btConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            btService = (binder as BluetoothService.LocalBinder).getService()
            observeBtService()
            
            // Trigger Auto-Connect on Startup
            val prefs = getApplication<Application>().getSharedPreferences("app_settings", 0)
            if (prefs.getBoolean("auto_connect", true)) {
                autoConnectAtStartup()
            }
        }

        override fun onServiceDisconnected(name: ComponentName) {
            btService = null
            _connectionState.value = ConnectionState.Disconnected("BT Service disconnected")
        }
    }

    init {
        try {
            val ctx = application
            val prefs = ctx.getSharedPreferences("app_settings", 0)
            pollIntervalMs = prefs.getInt("poll_interval", 100).toLong()
            _ecuBrand.value = prefs.getInt("ecu_brand", 0)

            ctx.bindService(Intent(ctx, BluetoothService::class.java), btConnection, Context.BIND_AUTO_CREATE)
            
            Log.d(TAG, "ViewModel initialized (Bluetooth Only)")
        } catch (e: Exception) {
            Log.e(TAG, "ViewModel init failed", e)
        }
    }

    private fun autoConnectAtStartup() {
        val bluetoothManager = getApplication<Application>().getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = bluetoothManager.adapter ?: return
        
        try {
            val bondedDevices = adapter.bondedDevices
            val target = bondedDevices.find { it.address.equals(TARGET_ECU_MAC, ignoreCase = true) }
            
            if (target != null) {
                Log.d(TAG, "Auto-connecting to ECU: ${target.address}")
                connect(target.address)
            } else {
                // If specific MAC not found, try last used address
                val lastAddress = getApplication<Application>()
                    .getSharedPreferences("app_settings", 0)
                    .getString("last_bt_address", "")
                
                if (!lastAddress.isNullOrEmpty()) {
                    Log.d(TAG, "Auto-connecting to last device: $lastAddress")
                    connect(lastAddress)
                }
            }
        } catch (e: SecurityException) {
            Log.e(TAG, "Bluetooth permissions missing for auto-connect")
        }
    }

    private fun observeBtService() {
        val s = btService ?: return
        viewModelScope.launch {
            s.connectionState.collect { state ->
                _connectionState.value = state
                if (state is ConnectionState.Connected) {
                    _statusMessage.value = "BT Connected: ${state.host}"
                    startPolling()
                } else if (state is ConnectionState.Disconnected) {
                    stopStream()
                    stopPolling()
                }
            }
        }

        viewModelScope.launch {
            s.responseFlow.collect { raw ->
                if (raw.isNotEmpty()) {
                    addToTerminal("RX: $raw")
                    handleResponse(SDSProtocol.parseResponse(raw))
                }
            }
        }
    }

    private fun handleResponse(response: ParsedResponse) {
        when (response) {
            is ParsedResponse.Status -> {
                _liveData.value = SDSProtocol.sdsDataFromStatus(response)
                _statusMessage.value = "Status updated"
            }
            is ParsedResponse.DTCList -> {
                _dtcList.value = response.codes
                _statusMessage.value = "DTCs: ${response.codes.size} found"
            }
            is ParsedResponse.ECUInfo -> {
                _ecuInfo.value = SDSEcuInfo(
                    vin = response.vin,
                    flashSize = response.flashSize,
                    calOffset = response.calOffset,
                    calSize = response.calSize
                )
                _statusMessage.value = "ECU Info received"
            }
            is ParsedResponse.StreamData -> {
                _liveData.value = SDSProtocol.sdsDataFromStatus(response.status)
            }
            is ParsedResponse.Pong -> {
                _statusMessage.value = "Pong"
            }
            is ParsedResponse.Loopback -> {
                _loopbackResult.value = response.data
                _statusMessage.value = "Loopback: ${response.data}"
                _isLoopbackRunning.value = false
                startPolling()
            }
            is ParsedResponse.Ok -> {
                _statusMessage.value = "OK: ${response.message}"
            }
            is ParsedResponse.Error -> {
                _statusMessage.value = "Error: ${response.message}"
            }
            is ParsedResponse.Stopped -> {
                _statusMessage.value = "Communication stopped"
                stopStream()
            }
            is ParsedResponse.Unknown -> {
                _statusMessage.value = "Unknown: ${response.raw}"
            }
            is ParsedResponse.Empty -> {}
        }
    }

    fun updatePollInterval(ms: Int) {
        pollIntervalMs = ms.toLong()
        if (_connectionState.value is ConnectionState.Connected) {
            restartPolling()
        }
    }

    private fun startPolling() {
        if (_isLoopbackRunning.value == true || isPollingPaused) return
        stopPolling()
        pollJob = viewModelScope.launch {
            try {
                while (isActive) {
                    if (_connectionState.value is ConnectionState.Connected && !isPollingPaused) {
                        sendCommand(SDSProtocol.encodeStatusRequest().decodeToString(), silent = true)
                    }
                    delay(pollIntervalMs)
                }
            } catch (_: Exception) {
            }
        }
    }

    fun togglePolling(): Boolean {
        isPollingPaused = !isPollingPaused
        if (isPollingPaused) stopPolling() else startPolling()
        return isPollingPaused
    }

    private fun stopPolling() {
        pollJob?.cancel()
        pollJob = null
    }

    private fun restartPolling() {
        startPolling()
    }

    fun connect(address: String? = null) {
        val target = address ?: getApplication<Application>()
            .getSharedPreferences("app_settings", 0)
            .getString("last_bt_address", "") ?: ""
        
        if (target.isNotEmpty()) {
            btService?.connect(target)
        } else {
            _statusMessage.value = "No BT address set"
        }
    }

    fun disconnect() {
        btService?.disconnect()
        _connectionState.value = ConnectionState.Disconnected()
        _statusMessage.value = "Disconnected"
    }

    fun requestDTC() {
        sendCommand(SDSProtocol.encodeDTCRequest().decodeToString())
    }

    fun requestECUInfo() {
        sendCommand(SDSProtocol.encodeInfoRequest().decodeToString())
    }

    fun dealerModeOn() {
        sendCommand(SDSProtocol.encodeDealerOn().decodeToString())
        _statusMessage.value = "Dealer mode ON"
    }

    fun dealerModeOff() {
        sendCommand(SDSProtocol.encodeDealerOff().decodeToString())
        _statusMessage.value = "Dealer mode OFF"
    }

    fun ping() {
        sendCommand(SDSProtocol.encodePing().decodeToString())
        _statusMessage.value = "Ping sent"
    }

    fun startStream() {
        if (_isStreaming.value == true) return
        _isStreaming.value = true
        sendCommand(SDSProtocol.encodeStreamStart().decodeToString())
        streamJob = viewModelScope.launch {
            while (_isStreaming.value == true) {
                delay(5000)
                sendCommand(SDSProtocol.encodeStatusRequest().decodeToString())
            }
        }
    }

    fun stopStream() {
        _isStreaming.value = false
        streamJob?.cancel()
        sendCommand(SDSProtocol.encodeStreamStop().decodeToString())
    }

    fun sendCommand(cmd: String, silent: Boolean = false) {
        btService?.sendCommand(cmd)
        if (!silent) {
            addToTerminal("TX: $cmd")
        }
        _statusMessage.value = "Sent: $cmd"
    }

    private fun addToTerminal(msg: String) {
        val timestamp = java.text.SimpleDateFormat("HH:mm:ss.SSS", java.util.Locale.US).format(java.util.Date())
        val fullMsg = "[$timestamp] $msg"
        val current = _terminalLog.value ?: emptyList()
        _terminalLog.value = (current + fullMsg).takeLast(100) // Keep last 100 lines
    }

    fun clearTerminal() {
        _terminalLog.value = emptyList()
    }

    fun checkForUpdates() {
        viewModelScope.launch {
            val manager = com.suzuki.ecutool.util.UpdateManager(getApplication())
            val info = manager.checkForUpdates()
            if (info != null) {
                manager.downloadInBackground(info, object : com.suzuki.ecutool.util.UpdateManager.UpdateListener {
                    override fun onUpdateReady(info: com.suzuki.ecutool.util.UpdateInfo, apkFile: File) {
                        _updateReady.postValue(apkFile)
                    }
                })
            }
        }
    }

    fun installUpdate(file: File) {
        val manager = com.suzuki.ecutool.util.UpdateManager(getApplication())
        manager.installApk(file)
    }

    fun sendStop() {
        sendCommand(SDSProtocol.encodeStop().decodeToString())
        _statusMessage.value = "Stop sent"
        stopStream()
    }

    fun triggerPreset(name: String) {
        _presetSignal.value = name
    }

    fun runLoopbackTest(data: String = "TEST1234") {
        _isLoopbackRunning.value = true
        stopPolling()
        sendCommand("kline_test_loopback")
    }

    fun sendPing() {
        sendCommand("ping")
    }

    fun setBrand(brandId: Int) {
        _ecuBrand.value = brandId
        getApplication<Application>().getSharedPreferences("app_settings", 0).edit()
            .putInt("ecu_brand", brandId).apply()
        
        val brandCmd = if (brandId == 1) "set_brand kawasaki" else "set_brand suzuki"
        sendCommand(brandCmd)
    }

    fun clearStatusMessage() {
        _statusMessage.value = ""
    }

    override fun onCleared() {
        stopStream()
        stopPolling()
        try { 
            getApplication<Application>().unbindService(btConnection)
        } catch (_: Exception) {}
        super.onCleared()
    }
}
