package com.suzuki.ecutool.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.tabs.TabLayout
import android.widget.Toast
import com.google.android.material.tabs.TabLayoutMediator
import com.suzuki.ecutool.R
import com.suzuki.ecutool.service.ConnectionState
import com.suzuki.ecutool.ui.dashboard.DashboardEngineFragment

class MainActivity : AppCompatActivity() {

    companion object {
        const val BUILD_VERSION = "1.1.5"
    }

    private val viewModel: MainViewModel by viewModels()
    private val configManager by lazy { DashConfigManager(this) }

    private lateinit var viewPager: ViewPager2
    private lateinit var tabLayout: TabLayout

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        if (permissions.all { it.value }) {
            Toast.makeText(this, "Permissions granted", Toast.LENGTH_SHORT).show()
        } else {
            Toast.makeText(this, "Permissions denied. BT may not work.", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        checkPermissions()

        viewPager = findViewById(R.id.viewPager)
        tabLayout = findViewById(R.id.tabLayout)

        setupViewPager()
        setupObservers()

        viewModel.checkForUpdates()
    }

    private fun checkPermissions() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            permissions.add(Manifest.permission.BLUETOOTH)
            permissions.add(Manifest.permission.BLUETOOTH_ADMIN)
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        val toRequest = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (toRequest.isNotEmpty()) {
            requestPermissionLauncher.launch(toRequest.toTypedArray())
        }
    }

    private fun setupViewPager() {
        val fragments = listOf(
            DashboardEngineFragment(), LiveDataFragment(), DiagnosticsFragment(),
            ECUToolsFragment(), TerminalFragment(), SettingsFragment(), AboutFragment()
        )
        viewPager.adapter = PageAdapter(this, fragments)
        viewPager.offscreenPageLimit = 1

        TabLayoutMediator(tabLayout, viewPager) { tab, pos ->
            tab.text = when (pos) {
                0 -> getString(R.string.dashboard)
                1 -> getString(R.string.live_data)
                2 -> getString(R.string.diagnostics)
                3 -> getString(R.string.ecu_tools)
                4 -> "Terminal"
                5 -> getString(R.string.settings)
                6 -> getString(R.string.about)
                else -> ""
            }
        }.attach()
    }

    fun applyGaugeTheme(name: String) {
        configManager.setTheme(name)
        setupViewPager()
    }

    fun applyDashLayout(name: String) {
        configManager.setLayout(name)
        setupViewPager()
    }

    fun applyDashConfig(config: DashConfig) {
        configManager.save(config)
        setupViewPager()
    }

    private fun setupObservers() {
        viewModel.connectionState.observe(this) { state ->
            when (state) {
                is ConnectionState.Connected -> {
                    Toast.makeText(this, "Connected to ${state.host}", Toast.LENGTH_SHORT).show()
                }
                is ConnectionState.Error -> {
                    Toast.makeText(this, "Connection Error: ${state.message}", Toast.LENGTH_LONG).show()
                }
                else -> {}
            }
        }

        viewModel.updateReady.observe(this) { file ->
            if (file != null) {
                androidx.appcompat.app.AlertDialog.Builder(this)
                    .setTitle("Update Downloaded")
                    .setMessage("A new version of the ECU Tool has been downloaded. Would you like to install it and restart now?")
                    .setPositiveButton("Install") { _, _ -> viewModel.installUpdate(file) }
                    .setNegativeButton("Later", null)
                    .setCancelable(false)
                    .show()
            }
        }
    }
}
