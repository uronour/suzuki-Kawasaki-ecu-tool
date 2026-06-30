package com.suzuki.ecutool.util

import android.app.DownloadManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.util.Log
import android.widget.Toast
import androidx.core.content.FileProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.net.URL

class UpdateManager(private val context: Context) {

    interface UpdateListener {
        fun onUpdateReady(info: UpdateInfo, apkFile: File)
    }

    companion object {
        private const val TAG = "UpdateManager"
        private const val UPDATE_FILE = "version.json"
        private const val APK_FILE = "app-debug.apk"
    }

    suspend fun checkForUpdates(): UpdateInfo? = withContext(Dispatchers.IO) {
        try {
            val serverHost = "suzuki-ecu.servebeer.com"
            val url = URL("http://$serverHost/$UPDATE_FILE")
            val connection = url.openConnection()
            connection.connectTimeout = 5000
            val text = connection.getInputStream().bufferedReader().use { it.readText() }
            val json = JSONObject(text)
            
            val latestVersion = json.getString("versionName")
            val latestCode = json.getInt("versionCode")
            
            val currentCode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                context.packageManager.getPackageInfo(context.packageName, 0).longVersionCode.toInt()
            } else {
                @Suppress("DEPRECATION")
                context.packageManager.getPackageInfo(context.packageName, 0).versionCode
            }
            
            Log.d(TAG, "Update check: Server $latestCode, Local $currentCode")
            
            if (latestCode > currentCode) {
                val downloadUrl = if (json.getString("url").startsWith("http")) {
                    json.getString("url")
                } else {
                    "http://$serverHost/${json.getString("url")}"
                }
                return@withContext UpdateInfo(latestVersion, latestCode, downloadUrl)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Update check failed: ${e.message}")
        }
        null
    }

    fun downloadInBackground(info: UpdateInfo, listener: UpdateListener) {
        val destinationFile = File(context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS), "update_${info.versionCode}.apk")
        if (destinationFile.exists()) {
            // If already downloaded, notify immediately
            listener.onUpdateReady(info, destinationFile)
            return
        }

        val request = DownloadManager.Request(Uri.parse(info.url))
            .setTitle("Downloading Suzuki ECU Update")
            .setDescription("Version ${info.versionName}")
            .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
            .setDestinationUri(Uri.fromFile(destinationFile))

        val dm = context.getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager
        val downloadId = dm.enqueue(request)

        val receiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                val id = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1)
                if (id == downloadId) {
                    Log.d(TAG, "Silent download complete")
                    listener.onUpdateReady(info, destinationFile)
                    context.unregisterReceiver(this)
                }
            }
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            context.registerReceiver(receiver, IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE), Context.RECEIVER_EXPORTED)
        } else {
            context.registerReceiver(receiver, IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE))
        }
    }

    fun installApk(file: File) {
        if (!file.exists()) return

        val uri = FileProvider.getUriForFile(context, "${context.packageName}.provider", file)
        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, "application/vnd.android.package-archive")
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        context.startActivity(intent)
    }
}

data class UpdateInfo(val versionName: String, val versionCode: Int, val url: String)
