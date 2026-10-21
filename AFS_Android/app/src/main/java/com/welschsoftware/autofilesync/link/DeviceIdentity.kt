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

import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyInfo
import android.security.keystore.KeyProperties
import java.net.Socket
import java.security.KeyFactory
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.MessageDigest
import java.security.Principal
import java.security.PrivateKey
import java.security.cert.X509Certificate
import java.security.spec.ECGenParameterSpec
import java.util.Calendar
import javax.net.ssl.X509KeyManager
import javax.security.auth.x500.X500Principal

object DeviceIdentity {  // the phone's TLS client certificate, made and kept by the Android keystore
    private const val KEYSTORE = "AndroidKeyStore"
    private const val ALIAS = "afs-device"
    private const val VALID_YEARS = 20

    private val keyStore: KeyStore by lazy { KeyStore.getInstance(KEYSTORE).apply { load(null) } }

    fun certificate(): X509Certificate {
        if (keyStore.containsAlias(ALIAS) && !usableForTls()) keyStore.deleteEntry(ALIAS)  // a key from an earlier build without DIGEST_NONE cannot sign a handshake
        if (!keyStore.containsAlias(ALIAS)) create()
        return keyStore.getCertificate(ALIAS) as X509Certificate
    }

    private fun usableForTls(): Boolean = runCatching {
        val key = keyStore.getKey(ALIAS, null) as PrivateKey
        val info = KeyFactory.getInstance(key.algorithm, KEYSTORE).getKeySpec(key, KeyInfo::class.java)
        KeyProperties.DIGEST_NONE in info.digests
    }.getOrDefault(false)

    fun fingerprint(): String = fingerprintOf(certificate())

    fun fingerprintOf(certificate: X509Certificate): String =
        MessageDigest.getInstance("SHA-256").digest(certificate.encoded).joinToString("") { "%02x".format(it) }

    fun keyManager(): X509KeyManager = object : X509KeyManager {  // always offers our one certificate, whatever CA list the PC sends (it sends none)
        override fun chooseClientAlias(keyType: Array<out String>?, issuers: Array<out Principal>?, socket: Socket?) = ALIAS
        override fun chooseServerAlias(keyType: String?, issuers: Array<out Principal>?, socket: Socket?) = null
        override fun getCertificateChain(alias: String?) = arrayOf(certificate())
        override fun getClientAliases(keyType: String?, issuers: Array<out Principal>?) = arrayOf(ALIAS)
        override fun getServerAliases(keyType: String?, issuers: Array<out Principal>?) = null
        override fun getPrivateKey(alias: String?) = privateKey()
    }

    private fun privateKey(): PrivateKey? {
        certificate()
        return keyStore.getKey(ALIAS, null) as? PrivateKey ?: run {
            keyStore.load(null)  // a keystore loaded before the key was generated does not see the key until reloaded
            keyStore.getKey(ALIAS, null) as? PrivateKey
        }
    }

    private fun create() {
        val notAfter = Calendar.getInstance().apply { add(Calendar.YEAR, VALID_YEARS) }
        val spec = KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_SIGN or KeyProperties.PURPOSE_VERIFY)
            .setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
            .setDigests(KeyProperties.DIGEST_NONE, KeyProperties.DIGEST_SHA256, KeyProperties.DIGEST_SHA384, KeyProperties.DIGEST_SHA512)  // TLS hashes the transcript itself and asks the key to sign the digest
            .setCertificateSubject(X500Principal("CN=AutoFileSync ${Build.MODEL}"))
            .setCertificateNotAfter(notAfter.time)
            .build()
        KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, KEYSTORE).apply { initialize(spec) }.generateKeyPair()
    }
}
