/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Android, the Android app of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
package com.welschsoftware.autofilesync.spike

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.net.wifi.WifiManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.util.Log
import android.view.WindowManager
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.net.NetworkInterface
import java.util.concurrent.Executors

class MainActivity : Activity()
{
    private val worker = Executors.newSingleThreadExecutor()
    private val server by lazy { SpikeServer(filesDir) }
    private lateinit var output: TextView
    private val wifi_lock by lazy { (applicationContext.getSystemService(WIFI_SERVICE) as WifiManager).createWifiLock(WifiManager.WIFI_MODE_FULL_LOW_LATENCY, TAG) }

    override fun onCreate(state: Bundle?)
    {
        super.onCreate(state)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)  // the spike server lives only while the activity does
        output = TextView(this)
        val column = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        column.addView(button("All files access") { request_all_files() })
        column.addView(button("mtime probe") { run("mtime") })
        column.addView(button("Start SFTP") { run("start") })
        column.addView(button("Stop SFTP") { run("stop") })
        column.addView(ScrollView(this).apply { addView(output) })
        setContentView(column)
        report("Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT}) ${Build.MANUFACTURER} ${Build.MODEL}")
        report("all files access=${Environment.isExternalStorageManager()} addresses=${addresses()}")
        intent?.getStringExtra("cmd")?.let(::run)
    }

    override fun onNewIntent(intent: Intent)
    {
        super.onNewIntent(intent)
        intent.getStringExtra("cmd")?.let(::run)
    }

    override fun onDestroy()
    {
        server.stop()
        if (wifi_lock.isHeld) wifi_lock.release()
        super.onDestroy()
    }

    private fun run(cmd: String) = worker.execute {
        try {
            when (cmd) {
                "mtime" -> MtimeProbe(::report).run()
                "start" -> report(server.start(PORT))
                "stop" -> server.stop().also { report("SFTP server stopped") }
                "lock" -> wifi_lock.acquire().also { report("wifi low latency lock held=${wifi_lock.isHeld}") }
                "unlock" -> wifi_lock.release().also { report("wifi lock released") }
                else -> report("unknown cmd ${cmd}")
            }
        } catch (e: Throwable) {
            report("FAIL ${cmd}: ${Log.getStackTraceString(e)}")
        }
    }

    private fun request_all_files()
    {
        startActivity(Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:${packageName}")))
    }

    private fun addresses() = NetworkInterface.getNetworkInterfaces().toList()
        .filter { it.isUp && !it.isLoopback }
        .flatMap { nic -> nic.inetAddresses.toList().map { "${nic.name}=${it.hostAddress}" } }

    private fun button(label: String, action: () -> Unit) = Button(this).apply { text = label; setOnClickListener { action() } }

    private fun report(text: String)
    {
        Log.i(TAG, text)
        runOnUiThread { output.append(text + "\n") }
    }

    companion object
    {
        const val TAG = "AFS-SPIKE"
        const val PORT = 52382
    }
}
