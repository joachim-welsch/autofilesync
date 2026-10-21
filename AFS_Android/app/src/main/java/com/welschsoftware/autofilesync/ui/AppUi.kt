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
package com.welschsoftware.autofilesync.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.welschsoftware.autofilesync.AppState
import com.welschsoftware.autofilesync.Health
import com.welschsoftware.autofilesync.R
import com.welschsoftware.autofilesync.Screen
import com.welschsoftware.autofilesync.link.SyncItemInfo

@Composable
fun AppRoot(state: AppState) {  // one screen at a time, chosen by the state; errors come and go as a snackbar
    val snackbar = remember { SnackbarHostState() }
    val context = LocalContext.current
    val message = state.message
    LaunchedEffect(message) {
        if (message != null) {
            val id = context.resources.getIdentifier(message, "string", context.packageName)
            snackbar.showSnackbar(if (id != 0) context.getString(id) else context.getString(R.string.error_generic, message))
            state.messageShown()
        }
    }
    Scaffold(snackbarHost = { SnackbarHost(snackbar) }) { padding ->
        Box(Modifier.fillMaxSize().padding(padding)) {
            when (val screen = state.screen) {
                Screen.Welcome -> Welcome(state)
                Screen.Scan -> Scan(state)
                is Screen.Confirm -> Confirm(state, screen.offer.pcName)
                Screen.Pairing -> Centered { CircularProgressIndicator(); Spacer(Modifier.height(16.dp)); Text(stringResource(R.string.pairing)) }
                Screen.Status -> Status(state)
            }
        }
    }
}

@Composable
private fun Centered(content: @Composable () -> Unit) {
    Column(Modifier.fillMaxSize().padding(24.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) { content() }
}

@Composable
private fun Welcome(state: AppState) {
    Centered {
        Text(stringResource(R.string.welcome_title), style = MaterialTheme.typography.headlineSmall)
        Spacer(Modifier.height(16.dp))
        Text(stringResource(R.string.welcome_text), style = MaterialTheme.typography.bodyLarge)
        Spacer(Modifier.height(32.dp))
        Button(onClick = state::startScan) { Text(stringResource(R.string.scan_button)) }
    }
}

@Composable
private fun Scan(state: AppState) {
    Column(Modifier.fillMaxSize()) {
        Box(Modifier.weight(1f)) { QrScanner(onResult = state::onScanned) }
        Row(Modifier.fillMaxWidth().padding(16.dp), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text(stringResource(R.string.scan_hint), Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
            TextButton(onClick = state::cancel) { Text(stringResource(R.string.cancel)) }
        }
    }
}

@Composable
private fun Confirm(state: AppState, pcName: String) {
    Centered {
        Text(stringResource(R.string.confirm_title, pcName), style = MaterialTheme.typography.headlineSmall)
        Spacer(Modifier.height(16.dp))
        Text(stringResource(R.string.confirm_text), style = MaterialTheme.typography.bodyLarge)
        Spacer(Modifier.height(32.dp))
        Button(onClick = state::confirmPair) { Text(stringResource(R.string.connect)) }
        TextButton(onClick = state::cancel) { Text(stringResource(R.string.cancel)) }
    }
}

@Composable
private fun Status(state: AppState) {
    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp)) {
                Text(stringResource(R.string.status_connected, state.pcName), style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(4.dp))
                Text(if (state.lastContact == 0L) stringResource(R.string.status_never) else stringResource(R.string.status_last_contact, ago(state.lastContact)), style = MaterialTheme.typography.bodyMedium)
                Spacer(Modifier.height(4.dp))
                val bad = state.health == Health.UNREACHABLE || state.health == Health.REJECTED || state.health == Health.DONE_WITH_ERRORS || state.health == Health.INCOMPLETE
                Text(when (state.health) {
                    Health.UP_TO_DATE -> stringResource(R.string.status_up_to_date)
                    Health.SYNCING -> if (state.progressTotal > 0) stringResource(R.string.syncing, state.progressDone, state.progressTotal) else stringResource(R.string.syncing_start)
                    Health.DONE -> stringResource(R.string.status_done, ago(state.lastContact))
                    Health.DONE_WITH_ERRORS -> stringResource(R.string.status_done_errors, ago(state.lastContact), state.errors)
                    Health.INCOMPLETE -> stringResource(R.string.status_incomplete)
                    Health.UNREACHABLE -> stringResource(R.string.status_unreachable)
                    Health.REJECTED -> stringResource(R.string.status_rejected)
                    Health.UNKNOWN -> ""
                }, style = MaterialTheme.typography.bodyMedium, color = if (bad) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface)
            }
        }
        if (!state.allFilesAccess) {
            Spacer(Modifier.height(16.dp))
            Card(Modifier.fillMaxWidth()) {  // without this permission the PC cannot read or write the phone's folders
                Column(Modifier.padding(16.dp)) {
                    Text(stringResource(R.string.access_title), style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(4.dp))
                    Text(stringResource(R.string.access_text), style = MaterialTheme.typography.bodyMedium)
                    Spacer(Modifier.height(8.dp))
                    Button(onClick = state::openAccessSettings) { Text(stringResource(R.string.access_button)) }
                }
            }
        }
        Spacer(Modifier.height(24.dp))
        Text(stringResource(R.string.items_title), style = MaterialTheme.typography.titleSmall)
        Spacer(Modifier.height(8.dp))
        if (state.items.isEmpty())
            Text(stringResource(R.string.items_none), style = MaterialTheme.typography.bodyMedium)
        else
            state.items.forEach { ItemRow(it) }
        Spacer(Modifier.weight(1f))
        Button(onClick = state::syncNow, enabled = !state.busy, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.sync_now)) }
        Spacer(Modifier.height(8.dp))
        Text(stringResource(R.string.settings_on_pc), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = state::forgetPc, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.forget_pc)) }
    }
}

@Composable
private fun ItemRow(item: SyncItemInfo) {
    Row(Modifier.fillMaxWidth().padding(vertical = 6.dp), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(item.name, style = MaterialTheme.typography.bodyLarge)
        Text(stringResource(when (item.direction) { "toPhone" -> R.string.direction_to_phone; "both" -> R.string.direction_both; else -> R.string.direction_to_pc }), style = MaterialTheme.typography.bodyMedium)
    }
    HorizontalDivider()
}

@Composable
private fun ago(epochMillis: Long): String {
    val minutes = (System.currentTimeMillis() - epochMillis) / 60000
    return when {
        minutes < 1 -> stringResource(R.string.just_now)
        minutes < 60 -> stringResource(R.string.minutes_ago, minutes.toInt())
        minutes < 60 * 24 -> stringResource(R.string.hours_ago, (minutes / 60).toInt())
        else -> stringResource(R.string.days_ago, (minutes / (60 * 24)).toInt())
    }
}
