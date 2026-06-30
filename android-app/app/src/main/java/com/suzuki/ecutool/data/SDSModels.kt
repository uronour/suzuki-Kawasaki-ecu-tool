package com.suzuki.ecutool.data

/**
 * SDS Protocol data model matching the firmware SDS_Data structure.
 * Values are already scaled to standard units by the firmware.
 */
data class SDSData(
    val rpm: Int = 0,
    val speedKmh: Int = 0,           // km/h
    val coolantTemp: Int = 0,        // Celsius
    val intakeAirTemp: Int = 0,      // Celsius
    val throttlePos: Int = 0,        // percent 0-100
    val batteryVolt: Int = 0,        // tenths of V: 142 = 14.2V
    val gearPos: Int = 0,            // 0=N, 1-6
    val mapKpa: Int = 0,
    val o2Sensor: Int = 0,
    val stps: Int = 0,               // percent 0-100
    val injectorPulse: Int = 0,      // raw pulse
    val ignitionTiming: Int = 0,     // degrees BTDC
    val iacStep: Int = 0,
    val baroKpa: Int = 0,
    val injectorPW: IntArray = intArrayOf(0, 0, 0, 0), // ms * 10
    val pairValve: Int = 0,
    val neutral: Boolean = false,
    val clutchIn: Boolean = false,
    val fanOn: Boolean = false,
    val sidestandDown: Boolean = false
) {
    fun batteryVoltage(): Float = batteryVolt / 10f
    fun speedKmh(): Float = speedKmh.toFloat()
    fun speedMph(): Float = speedKmh * 0.621371f
    fun injectorPulseMs(): Float = injectorPulse / 10f
    fun injectorPWMs(index: Int): Float = if (index in injectorPW.indices) injectorPW[index] / 10f else 0f
}

data class SDSEcuInfo(
    val ecuRawId: IntArray = intArrayOf(0, 0, 0, 0),
    val vin: String = "",
    val flashSize: Long = 0,
    val calOffset: Long = 0,
    val calSize: Long = 0
)

data class DTC(
    val code: Int,
    val description: String = ""
) {
    val displayCode: String get() = "C%02d".format(code)
}

sealed class SDSCommand {
    data class Status(val requestId: Int) : SDSCommand()
    data class DTC(val requestId: Int) : SDSCommand()
    data class DealerOn(val requestId: Int) : SDSCommand()
    data class DealerOff(val requestId: Int) : SDSCommand()
    data class ReadECUInfo(val requestId: Int) : SDSCommand()
    data class ReadMemory(val requestId: Int, val address: Long, val length: Int) : SDSCommand()
    data class WriteMemory(val requestId: Int, val address: Long, val data: ByteArray) : SDSCommand()
    data class DumpCalibration(val requestId: Int) : SDSCommand()
    data class StopCommunication(val requestId: Int) : SDSCommand()
    data class Ping(val requestId: Int) : SDSCommand()
    data class Stream(val requestId: Int, val enable: Boolean) : SDSCommand()
    data class Custom(val requestId: Int, val command: String) : SDSCommand()
}

sealed class SDSResponse {
    data class Status(val requestId: Int, val data: SDSData) : SDSResponse()
    data class DTCList(val requestId: Int, val dtcs: List<DTC>) : SDSResponse()
    data class ECUInfo(val requestId: Int, val info: SDSEcuInfo) : SDSResponse()
    data class MemoryData(val requestId: Int, val address: Long, val data: ByteArray) : SDSResponse()
    data class WriteResult(val requestId: Int, val success: Boolean, val message: String) : SDSResponse()
    data class DumpResult(val requestId: Int, val success: Boolean, val message: String, val size: Long) : SDSResponse()
    data class StopResult(val requestId: Int, val success: Boolean, val message: String) : SDSResponse()
    data class Ping(val requestId: Int) : SDSResponse()
    data class StreamData(val data: SDSData) : SDSResponse()
    data class Error(val requestId: Int, val message: String) : SDSResponse()
    data class Custom(val requestId: Int, val response: String) : SDSResponse()
}
