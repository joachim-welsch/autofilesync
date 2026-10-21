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

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.IOException
import java.net.InetSocketAddress
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket
import javax.net.ssl.X509TrustManager

class PcLink(private val store: PcStore, private val hostKey: SshHostKey, private val deviceName: String) {  // the phone's end of the control channel, see DeviceLink.md#protocol in AFS_Core

    sealed class Result {
        data class Ok(val lines: List<JSONObject>) : Result()
        data class Failed(val reason: String) : Result()  // unreachable, handshake, or the PC's error reason
    }

    suspend fun pair(offer: PairingOffer): Result {
        val request = JSONObject()
            .put("type", "pair")
            .put("token", offer.token)
            .put("device", JSONObject().put("id", store.deviceId).put("name", deviceName).put("sshHostKey", hostKey.openSshPublicLine()).put("sftpPort", SFTP_PORT))
            .put("appVersion", APP_VERSION)
        return exchange(offer.addresses, offer.port, offer.fingerprint, request)
    }

    suspend fun hello(sftp: SftpServer.Access? = null, onLine: (JSONObject) -> Unit = {}): Result {  // with an open server the PC runs a pass and reports progress line by line
        val pc = store.pc ?: return Result.Failed("not_paired")
        val request = JSONObject()
            .put("type", "hello")
            .put("deviceId", store.deviceId)
            .put("sftpPort", sftp?.port ?: 0)
            .put("sftpUser", sftp?.user ?: "")
            .put("sftpPassword", sftp?.password ?: "")
            .put("serving", sftp != null)
            .put("appVersion", APP_VERSION)
        return exchange(pc.addresses, pc.port, pc.fingerprint, request, onLine)
    }

    private suspend fun exchange(addresses: List<String>, port: Int, fingerprint: String, request: JSONObject, onLine: (JSONObject) -> Unit = {}): Result = withContext(Dispatchers.IO) {
        var failure = "unreachable"
        for (address in addresses) {
            val tls = connect(address, port, fingerprint) ?: continue
            try {
                tls.startHandshake()
                tls.outputStream.write((request.toString() + "\n").toByteArray())
                tls.outputStream.flush()
                tls.soTimeout = PASS_TIMEOUT_MS  // a pass may take long between two lines; the PC sends progress meanwhile
                val lines = mutableListOf<JSONObject>()
                tls.inputStream.bufferedReader().use { reader ->
                    while (true) {
                        val line = reader.readLine() ?: break
                        if (line.isBlank()) continue
                        val json = JSONObject(line)
                        lines += json
                        onLine(json)
                    }
                }
                val error = lines.firstOrNull { it.optString("type") == "error" }
                return@withContext if (error != null) Result.Failed(error.optString("reason", "error")) else Result.Ok(lines)
            } catch (e: CertificateException) {
                Log.w(TAG, "$address:$port handshake refused", e)
                failure = "handshake"
            } catch (e: IOException) {
                Log.w(TAG, "$address:$port failed", e)
                failure = if (e.cause is CertificateException) "handshake" else failure
            } finally {
                tls.close()
            }
        }
        Result.Failed(failure)
    }

    private fun connect(address: String, port: Int, fingerprint: String): SSLSocket? = runCatching {  // a TLS socket made by the factory, not layered over a plain one: the layered kind cannot sign with a keystore key
        val context = SSLContext.getInstance("TLSv1.3")
        context.init(arrayOf(DeviceIdentity.keyManager()), arrayOf(pinning(fingerprint)), null)
        (context.socketFactory.createSocket() as SSLSocket).apply {
            useClientMode = true
            connect(InetSocketAddress(address, port), CONNECT_TIMEOUT_MS)
            soTimeout = READ_TIMEOUT_MS
        }
    }.getOrNull()

    private fun pinning(fingerprint: String) = object : X509TrustManager {  // the QR code named the certificate; nothing else is trusted
        override fun checkClientTrusted(chain: Array<out X509Certificate>?, authType: String?) = throw CertificateException("client side only")
        override fun checkServerTrusted(chain: Array<out X509Certificate>?, authType: String?) {
            val presented = chain?.firstOrNull() ?: throw CertificateException("no certificate")
            if (DeviceIdentity.fingerprintOf(presented) != fingerprint) throw CertificateException("fingerprint mismatch")
        }
        override fun getAcceptedIssuers(): Array<X509Certificate> = emptyArray()
    }

    companion object {
        private const val TAG = "AFS-PcLink"
        const val SFTP_PORT = 52382
        const val APP_VERSION = "0.1.0"
        private const val CONNECT_TIMEOUT_MS = 3000
        private const val READ_TIMEOUT_MS = 10000
        private const val PASS_TIMEOUT_MS = 10 * 60 * 1000  // silence between two lines of a running pass
    }
}
