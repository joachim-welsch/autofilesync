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
package com.welschsoftware.autofilesync

import android.app.Application
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.welschsoftware.autofilesync.link.PairedPc
import com.welschsoftware.autofilesync.link.PairingOffer
import com.welschsoftware.autofilesync.link.PcLink
import com.welschsoftware.autofilesync.link.PcStore
import com.welschsoftware.autofilesync.link.SftpServer
import com.welschsoftware.autofilesync.link.SshHostKey
import com.welschsoftware.autofilesync.link.SyncItemInfo
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject

sealed class Screen {
    data object Welcome : Screen()
    data object Scan : Screen()
    data class Confirm(val offer: PairingOffer) : Screen()
    data object Pairing : Screen()
    data object Status : Screen()
}

enum class Health { UNKNOWN, UP_TO_DATE, SYNCING, DONE, DONE_WITH_ERRORS, INCOMPLETE, UNREACHABLE, REJECTED }

class AppState(application: Application) : AndroidViewModel(application) {  // everything the screens show, and the actions behind their buttons
    private val store = PcStore(application)
    private val hostKey = SshHostKey(application)
    private val link = PcLink(store, hostKey, "${Build.MANUFACTURER} ${Build.MODEL}".trim())
    private val sftp = SftpServer(application, hostKey)

    var screen by mutableStateOf<Screen>(if (store.pc == null) Screen.Welcome else Screen.Status)
        private set
    var pcName by mutableStateOf(store.pc?.name ?: "")
        private set
    var lastContact by mutableStateOf(store.lastContact)
        private set
    var items by mutableStateOf(store.items)
        private set
    var health by mutableStateOf(Health.UNKNOWN)
        private set
    var progressDone by mutableStateOf(0)
        private set
    var progressTotal by mutableStateOf(0)
        private set
    var errors by mutableStateOf(0)
        private set
    var busy by mutableStateOf(false)
        private set
    var allFilesAccess by mutableStateOf(Environment.isExternalStorageManager())
        private set
    var message by mutableStateOf<String?>(null)  // a transient error for the snackbar, as a string resource name or reason
        private set

    fun startScan() { screen = Screen.Scan }
    fun cancel() { screen = if (store.pc == null) Screen.Welcome else Screen.Status }
    fun messageShown() { message = null }
    fun refreshAccess() { allFilesAccess = Environment.isExternalStorageManager() }

    fun openAccessSettings() {  // the system page where the user grants access to all files; there is no runtime prompt for it
        val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:${getApplication<Application>().packageName}"))
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        getApplication<Application>().startActivity(intent)
    }

    fun onLink(uri: Uri?) {  // an app link from the camera app, or the scanner's text
        val offer = PairingOffer.parse(uri) ?: return
        onOffer(offer)
    }

    fun onScanned(text: String) {
        val offer = PairingOffer.parse(text)
        if (offer == null) message = "error_code_invalid" else onOffer(offer)
    }

    private fun onOffer(offer: PairingOffer) {
        if (offer.expired) {
            message = "error_code_expired"
            cancel()
        } else
            screen = Screen.Confirm(offer)
    }

    fun confirmPair() {
        val offer = (screen as? Screen.Confirm)?.offer ?: return
        screen = Screen.Pairing
        busy = true
        viewModelScope.launch {
            when (val result = link.pair(offer)) {
                is PcLink.Result.Ok -> {
                    val pc = result.lines.firstOrNull()?.optJSONObject("pc")
                    store.pc = PairedPc(pc?.optString("name")?.takeIf { it.isNotBlank() } ?: offer.pcName, offer.fingerprint, offer.addresses, offer.port)
                    store.lastContact = System.currentTimeMillis()
                    pcName = store.pc?.name ?: offer.pcName
                    lastContact = store.lastContact
                    health = Health.UP_TO_DATE
                    screen = Screen.Status
                }
                is PcLink.Result.Failed -> {
                    message = when (result.reason) {
                        "unreachable" -> "error_unreachable"
                        "handshake" -> "error_handshake"
                        "token_invalid" -> "error_token_invalid"
                        "pairing_closed" -> "error_pairing_closed"
                        else -> result.reason
                    }
                    cancel()
                }
            }
            busy = false
        }
    }

    fun syncNow(serve: Boolean = true) {  // report in; with the file server open the PC runs the pass and reports progress
        if (store.pc == null || busy) return
        refreshAccess()
        if (serve && !allFilesAccess) {
            message = "error_no_file_access"
            return
        }
        busy = true
        progressDone = 0
        progressTotal = 0
        viewModelScope.launch {
            val access = if (serve) withContext(Dispatchers.IO) { sftp.start() } else null
            if (serve && access == null) {
                message = "error_server"
                busy = false
                return@launch
            }
            var done: JSONObject? = null
            val result = link.hello(access) { line ->
                when (line.optString("type")) {
                    "items" -> items = itemsOf(line).also { store.items = it }
                    "serve" -> health = Health.SYNCING
                    "progress" -> { progressDone = line.optInt("done"); progressTotal = line.optInt("total"); health = Health.SYNCING }
                    "done" -> done = line
                }
            }
            withContext(Dispatchers.IO) { sftp.stop() }
            when (result) {
                is PcLink.Result.Ok -> {
                    store.lastContact = System.currentTimeMillis()
                    lastContact = store.lastContact
                    errors = done?.optInt("errors") ?: 0
                    health = when {
                        done == null -> Health.UP_TO_DATE
                        done?.optString("completed") != "true" -> Health.INCOMPLETE
                        errors > 0 -> Health.DONE_WITH_ERRORS
                        else -> Health.DONE
                    }
                }
                is PcLink.Result.Failed -> health = if (result.reason == "unknown_device") Health.REJECTED else Health.UNREACHABLE
            }
            busy = false
        }
    }

    fun forgetPc() {
        store.pc = null
        pcName = ""
        items = emptyList()
        lastContact = 0L
        health = Health.UNKNOWN
        screen = Screen.Welcome
    }

    private fun itemsOf(line: JSONObject): List<SyncItemInfo> {
        val array = line.optJSONArray("items") ?: return emptyList()
        return (0 until array.length()).map { array.getJSONObject(it) }.map { SyncItemInfo(it.optString("name"), it.optString("direction", "toPc")) }
    }
}
