package com.suzuki.ecutool.service

sealed class ConnectionState {
    data class Disconnected(val reason: String? = null) : ConnectionState()
    object Connecting : ConnectionState()
    data class Connected(val host: String, val port: Int = 0) : ConnectionState()
    data class Error(val message: String) : ConnectionState()
}
