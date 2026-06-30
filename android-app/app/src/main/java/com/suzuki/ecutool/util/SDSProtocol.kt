package com.suzuki.ecutool.util

import com.suzuki.ecutool.data.SDSData

object SDSProtocol {

    private const val CMD_STATUS = "status"
    private const val CMD_DTC = "dtc"
    private const val CMD_DEALER_ON = "dealer_on"
    private const val CMD_DEALER_OFF = "dealer_off"
    private const val CMD_INFO = "info"
    private const val CMD_PING = "ping"
    private const val CMD_STREAM_ON = "stream_on"
    private const val CMD_STREAM_OFF = "stream_off"
    private const val CMD_STOP = "stop"
    private const val CMD_LOOPBACK = "loopback"
    private const val CMD_HELP = "help"

    fun encodeCommand(cmd: String, args: String = ""): ByteArray {
        val line = if (args.isEmpty()) cmd else "$cmd $args"
        return (line + "\r\n").toByteArray(Charsets.US_ASCII)
    }

    fun encodeStatusRequest(): ByteArray = encodeCommand(CMD_STATUS)
    fun encodeDTCRequest(): ByteArray = encodeCommand(CMD_DTC)
    fun encodeDealerOn(): ByteArray = encodeCommand(CMD_DEALER_ON)
    fun encodeDealerOff(): ByteArray = encodeCommand(CMD_DEALER_OFF)
    fun encodeInfoRequest(): ByteArray = encodeCommand(CMD_INFO)
    fun encodePing(): ByteArray = encodeCommand(CMD_PING)
    fun encodeStreamStart(): ByteArray = encodeCommand(CMD_STREAM_ON)
    fun encodeStreamStop(): ByteArray = encodeCommand(CMD_STREAM_OFF)
    fun encodeStop(): ByteArray = encodeCommand(CMD_STOP)
    fun encodeLoopback(data: String): ByteArray = encodeCommand(CMD_LOOPBACK, data)

    fun parseResponse(raw: String): ParsedResponse {
        val trimmed = raw.trim()
        if (trimmed.isEmpty()) return ParsedResponse.Empty

        val parts = trimmed.split(" ", limit = 2)
        val keyword = parts[0].lowercase()
        val body = parts.getOrElse(1) { "" }

        return when (keyword) {
            "status" -> parseStatus(body)
            "dtc" -> parseDTC(body)
            "info" -> parseInfo(body)
            "ping" -> ParsedResponse.Pong
            "loopback", "kline_loopback" -> ParsedResponse.Loopback(body)
            "error" -> ParsedResponse.Error(body)
            "ok" -> ParsedResponse.Ok(body)
            "stream" -> parseStream(body)
            "stopped" -> ParsedResponse.Stopped
            else -> ParsedResponse.Unknown(trimmed)
        }
    }

    private fun parseStatus(body: String): ParsedResponse {
        val pairs = body.split(",").associate {
            val kv = it.trim().split("=", limit = 2)
            kv[0].trim() to kv.getOrElse(1) { "0" }.trim()
        }
        return ParsedResponse.Status(
            rpm = pairs["rpm"]?.toIntOrNull() ?: 0,
            speedKmh = pairs["spd"]?.toIntOrNull() ?: 0,
            coolantTemp = pairs["cool"]?.toIntOrNull() ?: 0,
            intakeAirTemp = pairs["iat"]?.toIntOrNull() ?: 0,
            throttlePos = pairs["tps"]?.toIntOrNull() ?: 0,
            batteryVolt = pairs["bat"]?.toIntOrNull() ?: 0,
            gearPos = pairs["gear"]?.toIntOrNull() ?: 0,
            mapKpa = pairs["map"]?.toIntOrNull() ?: 0,
            o2Sensor = pairs["o2"]?.toIntOrNull() ?: 0,
            stps = pairs["stps"]?.toIntOrNull() ?: 0,
            injectorPulse = pairs["inj"]?.toIntOrNull() ?: 0,
            ignitionTiming = pairs["ign"]?.toIntOrNull() ?: 0,
            iacStep = pairs["iac"]?.toIntOrNull() ?: 0,
            baroKpa = pairs["baro"]?.toIntOrNull() ?: 0,
            neutral = pairs["neut"] == "1",
            clutchIn = pairs["clutch"] == "1",
            fanOn = pairs["fan"] == "1",
            sidestandDown = pairs["std"] == "1"
        )
    }

    private fun parseDTC(body: String): ParsedResponse {
        if (body.isBlank() || body.startsWith("none")) {
            return ParsedResponse.DTCList(emptyList())
        }
        val codes = body.split(",").mapNotNull { it.trim().toIntOrNull() }
        return ParsedResponse.DTCList(codes.map { com.suzuki.ecutool.data.DTC(it) })
    }

    private fun parseInfo(body: String): ParsedResponse {
        val pairs = body.split(",").associate {
            val kv = it.trim().split("=", limit = 2)
            kv[0].trim() to kv.getOrElse(1) { "" }.trim()
        }
        return ParsedResponse.ECUInfo(
            vin = pairs["vin"] ?: "",
            flashSize = pairs["flash"]?.toLongOrNull() ?: 0,
            calOffset = pairs["cal_off"]?.toLongOrNull() ?: 0,
            calSize = pairs["cal_sz"]?.toLongOrNull() ?: 0
        )
    }

    private fun parseStream(body: String): ParsedResponse {
        val st = parseStatus(body)
        return if (st is ParsedResponse.Status) {
            ParsedResponse.StreamData(st)
        } else {
            ParsedResponse.Unknown("stream $body")
        }
    }

    fun sdsDataFromStatus(status: ParsedResponse.Status): SDSData {
        return SDSData(
            rpm = status.rpm,
            speedKmh = status.speedKmh,
            coolantTemp = status.coolantTemp,
            intakeAirTemp = status.intakeAirTemp,
            throttlePos = status.throttlePos,
            batteryVolt = status.batteryVolt,
            gearPos = status.gearPos,
            mapKpa = status.mapKpa,
            o2Sensor = status.o2Sensor,
            stps = status.stps,
            injectorPulse = status.injectorPulse,
            ignitionTiming = status.ignitionTiming,
            iacStep = status.iacStep,
            baroKpa = status.baroKpa,
            neutral = status.neutral,
            clutchIn = status.clutchIn,
            fanOn = status.fanOn,
            sidestandDown = status.sidestandDown
        )
    }
}

sealed class ParsedResponse {
    data class Status(
        val rpm: Int,
        val speedKmh: Int,
        val coolantTemp: Int,
        val intakeAirTemp: Int,
        val throttlePos: Int,
        val batteryVolt: Int,
        val gearPos: Int,
        val mapKpa: Int,
        val o2Sensor: Int,
        val stps: Int,
        val injectorPulse: Int,
        val ignitionTiming: Int,
        val iacStep: Int,
        val baroKpa: Int,
        val neutral: Boolean,
        val clutchIn: Boolean,
        val fanOn: Boolean,
        val sidestandDown: Boolean
    ) : ParsedResponse()

    data class DTCList(val codes: List<com.suzuki.ecutool.data.DTC>) : ParsedResponse()
    data class ECUInfo(
        val vin: String,
        val flashSize: Long,
        val calOffset: Long,
        val calSize: Long
    ) : ParsedResponse()

    data class Loopback(val data: String) : ParsedResponse()

    data class StreamData(val status: Status) : ParsedResponse()
    object Pong : ParsedResponse()
    data class Ok(val message: String) : ParsedResponse()
    data class Error(val message: String) : ParsedResponse()
    object Stopped : ParsedResponse()
    data class Unknown(val raw: String) : ParsedResponse()
    object Empty : ParsedResponse()
}
