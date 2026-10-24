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

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.NetworkInterface
import java.net.SocketTimeoutException

object Discovery {  // finds the paired PC after its address changed, see DeviceLink.md#discovery in AFS_Core
    private const val PORT = 52381
    private const val TIMEOUT_MS = 1500
    private const val ATTEMPTS = 2

    data class Found(val address: String, val port: Int)

    suspend fun find(fingerprint: String): Found? = withContext(Dispatchers.IO) {
        runCatching {
            DatagramSocket().use { socket ->
                socket.broadcast = true
                socket.soTimeout = TIMEOUT_MS
                val request = "AFS-DISCOVER 1 $fingerprint".toByteArray()
                repeat(ATTEMPTS) {
                    broadcastAddresses().forEach { socket.send(DatagramPacket(request, request.size, it, PORT)) }
                    val found = receive(socket, fingerprint)
                    if (found != null) return@runCatching found
                }
                null
            }
        }.getOrNull()
    }

    private fun receive(socket: DatagramSocket, fingerprint: String): Found? {
        val buffer = ByteArray(256)
        while (true) {
            val packet = DatagramPacket(buffer, buffer.size)
            try {
                socket.receive(packet)
            } catch (e: SocketTimeoutException) {
                return null
            }
            val parts = String(packet.data, 0, packet.length).split(' ')
            if (parts.size == 4 && parts[0] == "AFS-HERE" && parts[2] == fingerprint)  // only the PC we paired with
                return Found(packet.address.hostAddress ?: continue, parts[3].trim().toIntOrNull() ?: continue)
        }
    }

    private fun broadcastAddresses(): List<InetAddress> {  // the subnet's own broadcast address gets through where 255.255.255.255 is filtered
        val subnets = NetworkInterface.getNetworkInterfaces().toList()
            .filter { it.isUp && !it.isLoopback }
            .flatMap { it.interfaceAddresses }
            .mapNotNull { it.broadcast }
        return (subnets + InetAddress.getByName("255.255.255.255")).distinct()
    }
}
