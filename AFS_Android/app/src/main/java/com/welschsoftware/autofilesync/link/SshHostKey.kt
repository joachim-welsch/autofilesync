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
import java.io.ByteArrayOutputStream
import java.io.DataOutputStream
import java.io.File
import java.math.BigInteger
import java.security.KeyFactory
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.interfaces.ECPublicKey
import java.security.spec.ECGenParameterSpec
import java.security.spec.PKCS8EncodedKeySpec
import java.security.spec.X509EncodedKeySpec
import java.util.Base64

class SshHostKey(context: Context) {  // the key the phone's SFTP server will present; its public half is sent at pairing
    private val privateFile = File(context.filesDir, "hostkey.pkcs8")
    private val publicFile = File(context.filesDir, "hostkey.x509")

    val keyPair: KeyPair by lazy { load() ?: create() }

    fun openSshPublicLine(): String {
        val key = keyPair.public as ECPublicKey
        val point = ByteArray(65).also { encoded ->
            encoded[0] = 4
            key.w.affineX.toUnsigned32().copyInto(encoded, 1)
            key.w.affineY.toUnsigned32().copyInto(encoded, 33)
        }
        val blob = ByteArrayOutputStream()
        DataOutputStream(blob).run {
            writeSshString(TYPE.toByteArray())
            writeSshString(CURVE.toByteArray())
            writeSshString(point)
        }
        return "$TYPE ${Base64.getEncoder().encodeToString(blob.toByteArray())}"
    }

    private fun DataOutputStream.writeSshString(bytes: ByteArray) {
        writeInt(bytes.size)
        write(bytes)
    }

    private fun BigInteger.toUnsigned32(): ByteArray {
        val raw = toByteArray()
        val out = ByteArray(32)
        val start = raw.size - 32
        if (start >= 0) raw.copyInto(out, 0, start) else raw.copyInto(out, -start)  // strip a sign byte or pad a short value
        return out
    }

    private fun load(): KeyPair? {
        if (!privateFile.exists() || !publicFile.exists()) return null
        return runCatching {
            val factory = KeyFactory.getInstance("EC")
            KeyPair(factory.generatePublic(X509EncodedKeySpec(publicFile.readBytes())), factory.generatePrivate(PKCS8EncodedKeySpec(privateFile.readBytes())))
        }.getOrNull()
    }

    private fun create(): KeyPair {
        val pair = KeyPairGenerator.getInstance("EC").apply { initialize(ECGenParameterSpec("secp256r1")) }.generateKeyPair()
        privateFile.writeBytes(pair.private.encoded)
        publicFile.writeBytes(pair.public.encoded)
        return pair
    }

    companion object {
        const val TYPE = "ecdsa-sha2-nistp256"
        const val CURVE = "nistp256"
    }
}
