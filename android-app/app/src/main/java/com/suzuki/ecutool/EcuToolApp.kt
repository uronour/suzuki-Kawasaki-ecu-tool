package com.suzuki.ecutool

import android.app.Application
import android.util.Log
import java.io.File
import java.io.FileWriter
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class EcuToolApp : Application() {

    private val oldHandler = Thread.getDefaultUncaughtExceptionHandler()

    override fun onCreate() {
        super.onCreate()
        Thread.setDefaultUncaughtExceptionHandler { thread, ex ->
            try {
                val f = File(filesDir, "crash_log.txt")
                val sw = StringWriter()
                sw.write("=== CRASH at ${SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())} ===\n")
                sw.write("Thread: ${thread.name}\n")
                sw.write("${ex.javaClass.simpleName}: ${ex.message}\n")
                ex.printStackTrace(PrintWriter(sw))
                sw.write("\n")
                FileWriter(f, true).use { it.write(sw.toString()) }
                Log.e("EcuToolApp", "Crash logged to ${f.absolutePath}", ex)
            } catch (_: Exception) {}
            oldHandler?.uncaughtException(thread, ex)
        }
    }
}
