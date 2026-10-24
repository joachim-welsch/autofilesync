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
package com.welschsoftware.autofilesync.link

import android.content.Context
import androidx.core.content.edit
import org.json.JSONArray
import java.security.SecureRandom

data class SyncItemInfo(val name: String, val direction: String)  // direction: toPc, toPhone, both

data class PairedPc(val name: String, val fingerprint: String, val addresses: List<String>, val port: Int)

class PcStore(context: Context) {  // what the app remembers between starts: its own id and the one PC it is paired with
    private val prefs = context.getSharedPreferences("pc", Context.MODE_PRIVATE)

    val deviceId: String
        get() = prefs.getString("deviceId", null) ?: ByteArray(16).also { SecureRandom().nextBytes(it) }
            .joinToString("") { "%02x".format(it) }.also { prefs.edit { putString("deviceId", it) } }

    var pc: PairedPc?
        get() {
            val fingerprint = prefs.getString("pcFingerprint", null) ?: return null
            return PairedPc(
                prefs.getString("pcName", "PC") ?: "PC",
                fingerprint,
                (prefs.getString("pcAddresses", "") ?: "").split(',').filter { it.isNotBlank() },
                prefs.getInt("pcPort", 0),
            )
        }
        set(value) = prefs.edit {
            if (value == null) {
                remove("pcName"); remove("pcFingerprint"); remove("pcAddresses"); remove("pcPort"); remove("lastContact"); remove("items")
            } else {
                putString("pcName", value.name); putString("pcFingerprint", value.fingerprint)
                putString("pcAddresses", value.addresses.joinToString(",")); putInt("pcPort", value.port)
            }
        }

    var wifiOnly: Boolean  // run passes in the background only on an unmetered network
        get() = prefs.getBoolean("wifiOnly", true)
        set(value) = prefs.edit { putBoolean("wifiOnly", value) }

    var chargingOnly: Boolean
        get() = prefs.getBoolean("chargingOnly", false)
        set(value) = prefs.edit { putBoolean("chargingOnly", value) }

    var unreachableNotified: Long  // epoch millis of the last "PC not reachable" notification
        get() = prefs.getLong("unreachableNotified", 0L)
        set(value) = prefs.edit { putLong("unreachableNotified", value) }

    fun moveTo(address: String, port: Int) {  // the PC was found under a new address (Discovery)
        val known = pc ?: return
        pc = known.copy(addresses = listOf(address) + known.addresses.filter { it != address }, port = port)
    }

    var lastContact: Long  // epoch millis, 0 = never
        get() = prefs.getLong("lastContact", 0L)
        set(value) = prefs.edit { putLong("lastContact", value) }

    var items: List<SyncItemInfo>
        get() {
            val array = JSONArray(prefs.getString("items", "[]") ?: "[]")
            return (0 until array.length()).map { array.getJSONObject(it) }.map { SyncItemInfo(it.optString("name"), it.optString("direction", "toPc")) }
        }
        set(value) = prefs.edit {
            putString("items", JSONArray().apply { value.forEach { put(org.json.JSONObject().put("name", it.name).put("direction", it.direction)) } }.toString())
        }
}
