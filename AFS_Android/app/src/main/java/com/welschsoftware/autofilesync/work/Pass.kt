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
package com.welschsoftware.autofilesync.work

import android.content.Context
import android.media.MediaScannerConnection
import android.net.wifi.WifiManager
import android.os.Build
import com.welschsoftware.autofilesync.link.Discovery
import com.welschsoftware.autofilesync.link.FileAccess
import com.welschsoftware.autofilesync.link.PcLink
import com.welschsoftware.autofilesync.link.PcStore
import com.welschsoftware.autofilesync.link.SftpServer
import com.welschsoftware.autofilesync.link.SshHostKey
import com.welschsoftware.autofilesync.link.SyncItemInfo
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONObject

enum class Outcome { NONE, RUNNING, CONTACTED, DONE, DONE_WITH_ERRORS, INCOMPLETE, UNREACHABLE, REJECTED, NO_FILE_ACCESS, SERVER_FAILED }

data class PassState(val outcome: Outcome = Outcome.NONE, val done: Int = 0, val total: Int = 0, val errors: Int = 0)

object Pass {  // one contact with the PC at a time, for the button and the background alike
    private val mutex = Mutex()
    private val mutableState = MutableStateFlow(PassState())
    val state: StateFlow<PassState> = mutableState

    suspend fun run(context: Context, serve: Boolean, onProgress: (Int, Int) -> Unit = { _, _ -> }): PassState = mutex.withLock {
        val app = context.applicationContext
        val store = PcStore(app)
        if (store.pc == null) return PassState()
        if (serve && !FileAccess.granted(app)) return publish(PassState(Outcome.NO_FILE_ACCESS))
        val hostKey = SshHostKey(app)
        val link = PcLink(store, hostKey, "${Build.MANUFACTURER} ${Build.MODEL}".trim())
        val server = SftpServer(app, hostKey)
        val wifiLock = (app.getSystemService(Context.WIFI_SERVICE) as WifiManager).createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "afs-pass")
        mutableState.value = PassState(Outcome.RUNNING)
        try {
            val access = if (serve) withContext(Dispatchers.IO) { server.start() } else null
            if (serve && access == null) return publish(PassState(Outcome.SERVER_FAILED))
            if (serve) wifiLock.acquire()  // keeps the Wi-Fi awake when the screen goes off mid-pass
            var result = contact(link, store, access, onProgress)
            if (result.first is PcLink.Result.Failed && (result.first as PcLink.Result.Failed).reason == "unreachable") {
                val found = Discovery.find(store.pc!!.fingerprint)  // the PC may have a new address
                if (found != null) {
                    store.moveTo(found.address, found.port)
                    result = contact(link, store, access, onProgress)
                }
            }
            return publish(stateOf(result.first, result.second, store))
        } finally {
            withContext(Dispatchers.IO) { server.stop() }
            if (wifiLock.isHeld) wifiLock.release()
            scanMedia(app, server.changedPaths())
        }
    }

    private suspend fun contact(link: PcLink, store: PcStore, access: SftpServer.Access?, onProgress: (Int, Int) -> Unit): Pair<PcLink.Result, JSONObject?> {
        var done: JSONObject? = null
        val result = link.hello(access) { line ->
            when (line.optString("type")) {
                "items" -> store.items = itemsOf(line)
                "progress" -> {
                    mutableState.value = PassState(Outcome.RUNNING, line.optInt("done"), line.optInt("total"))
                    onProgress(line.optInt("done"), line.optInt("total"))
                }
                "done" -> done = line
            }
        }
        return result to done
    }

    private fun stateOf(result: PcLink.Result, done: JSONObject?, store: PcStore): PassState = when (result) {
        is PcLink.Result.Failed -> PassState(if (result.reason == "unknown_device") Outcome.REJECTED else Outcome.UNREACHABLE)
        is PcLink.Result.Ok -> {
            store.lastContact = System.currentTimeMillis()
            val errors = done?.optInt("errors") ?: 0
            when {
                done == null -> PassState(Outcome.CONTACTED)
                done.optString("completed") != "true" -> PassState(Outcome.INCOMPLETE, errors = errors)
                errors > 0 -> PassState(Outcome.DONE_WITH_ERRORS, errors = errors)
                else -> PassState(Outcome.DONE)
            }
        }
    }

    private fun publish(state: PassState): PassState {
        mutableState.value = state
        return state
    }

    private fun scanMedia(context: Context, paths: List<String>) {  // the gallery and music apps see new or removed files only after a scan
        if (paths.isNotEmpty()) MediaScannerConnection.scanFile(context, paths.toTypedArray(), null, null)
    }

    private fun itemsOf(line: JSONObject): List<SyncItemInfo> {
        val array = line.optJSONArray("items") ?: return emptyList()
        return (0 until array.length()).map { array.getJSONObject(it) }.map { SyncItemInfo(it.optString("name"), it.optString("direction", "toPc")) }
    }
}
