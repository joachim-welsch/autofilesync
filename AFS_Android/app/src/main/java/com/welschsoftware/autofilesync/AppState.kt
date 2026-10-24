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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.welschsoftware.autofilesync.link.FileAccess
import com.welschsoftware.autofilesync.link.PairedPc
import com.welschsoftware.autofilesync.link.PairingOffer
import com.welschsoftware.autofilesync.link.PcLink
import com.welschsoftware.autofilesync.link.PcStore
import com.welschsoftware.autofilesync.link.SshHostKey
import com.welschsoftware.autofilesync.link.SyncItemInfo
import com.welschsoftware.autofilesync.work.Outcome
import com.welschsoftware.autofilesync.work.Pass
import com.welschsoftware.autofilesync.work.Scheduler
import kotlinx.coroutines.launch

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
    private val link = PcLink(store, SshHostKey(application), "${Build.MANUFACTURER} ${Build.MODEL}".trim())

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
    var allFilesAccess by mutableStateOf(FileAccess.granted(application))
        private set
    var wifiOnly by mutableStateOf(store.wifiOnly)
        private set
    var chargingOnly by mutableStateOf(store.chargingOnly)
        private set

    init {
        viewModelScope.launch { Pass.state.collect(::show) }  // the background passes report here as well
    }

    private fun show(state: com.welschsoftware.autofilesync.work.PassState) {
        progressDone = state.done
        progressTotal = state.total
        errors = state.errors
        busy = state.outcome == Outcome.RUNNING
        items = store.items
        lastContact = store.lastContact
        health = when (state.outcome) {
            Outcome.NONE -> health
            Outcome.RUNNING -> Health.SYNCING
            Outcome.CONTACTED -> Health.UP_TO_DATE
            Outcome.DONE -> Health.DONE
            Outcome.DONE_WITH_ERRORS -> Health.DONE_WITH_ERRORS
            Outcome.INCOMPLETE -> Health.INCOMPLETE
            Outcome.UNREACHABLE -> Health.UNREACHABLE
            Outcome.REJECTED -> Health.REJECTED
            Outcome.NO_FILE_ACCESS -> health.also { message = "error_no_file_access" }
            Outcome.SERVER_FAILED -> health.also { message = "error_server" }
        }
    }

    fun changeWifiOnly(value: Boolean) {
        store.wifiOnly = value
        wifiOnly = value
        Scheduler.ensure(getApplication())
    }

    fun changeChargingOnly(value: Boolean) {
        store.chargingOnly = value
        chargingOnly = value
        Scheduler.ensure(getApplication())
    }
    var message by mutableStateOf<String?>(null)  // a transient error for the snackbar, as a string resource name or reason
        private set

    fun startScan() { screen = Screen.Scan }
    fun cancel() { screen = if (store.pc == null) Screen.Welcome else Screen.Status }
    fun messageShown() { message = null }
    fun refreshAccess() { allFilesAccess = FileAccess.granted(getApplication()) }

    fun openAccessSettings() {  // a system page; Android has no in-app prompt for access to all files
        getApplication<Application>().startActivity(FileAccess.settingsIntent(getApplication()).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
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
                    Scheduler.ensure(getApplication())
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

    fun syncNow() {  // a pass at once, in the background so it continues when the app is left
        if (store.pc == null || busy) return
        refreshAccess()
        if (!allFilesAccess) {
            message = "error_no_file_access"
            return
        }
        Scheduler.runNow(getApplication())
    }

    fun reportIn() {  // when the user looks: fetch the item list without a pass
        if (store.pc == null || busy) return
        viewModelScope.launch { Pass.run(getApplication(), serve = false) }
    }

    fun forgetPc() {
        store.pc = null
        pcName = ""
        items = emptyList()
        lastContact = 0L
        health = Health.UNKNOWN
        screen = Screen.Welcome
        Scheduler.ensure(getApplication())  // cancels the background passes
    }
}
