package com.suzuki.ecutool.util

object DTCUtils {
    private val codeMap = mapOf(
        11 to "Camshaft Position Sensor (CMP)",
        12 to "Crankshaft Position Sensor (CKP)",
        13 to "Intake Air Pressure Sensor (IAP)",
        14 to "Throttle Position Sensor (TPS)",
        15 to "Engine Coolant Temperature Sensor (ECT)",
        21 to "Intake Air Temperature Sensor (IAT)",
        22 to "Atmospheric Pressure Sensor (AP)",
        23 to "Tip-over Sensor (TOS)",
        24 to "Ignition Signal #1 (Cyl 1)",
        25 to "Ignition Signal #2 (Cyl 2)",
        26 to "Ignition Signal #3 (Cyl 3)",
        27 to "Ignition Signal #4 (Cyl 4)",
        28 to "Secondary Throttle Valve Actuator (STVA)",
        29 to "Secondary Throttle Position Sensor (STPS)",
        31 to "Gear Position Sensor (GPS)",
        32 to "Injector Signal #1",
        33 to "Injector Signal #2",
        34 to "Injector Signal #3",
        35 to "Injector Signal #4",
        40 to "ISC Valve",
        41 to "Fuel Pump Relay",
        42 to "Ignition Switch Signal",
        44 to "HO2 Sensor",
        46 to "Exhaust Control Valve Actuator (EXCVA)",
        49 to "PAIR Control Solenoid Valve",
        60 to "Cooling Fan Relay"
    )

    fun getDescription(code: Int): String {
        return codeMap[code] ?: "Unknown Error Code"
    }

    fun getDisplayCode(code: Int): String {
        return "C%02d".format(code)
    }
}
