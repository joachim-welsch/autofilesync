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

import android.net.Uri
import java.net.URLDecoder

data class PairingOffer(  // what the PC's QR code carries, see DevicePairing.md#qr_content in AFS_Core
    val pcName: String,
    val addresses: List<String>,
    val port: Int,
    val fingerprint: String,
    val token: String,
) {  // the expiry in the code is left to the PC, whose clock it belongs to; a phone's clock may differ by hours
    companion object {
        const val HOST = "welsch-software.com"
        const val PATH = "/pair"

        fun parse(text: String): PairingOffer? = runCatching { parse(Uri.parse(text)) }.getOrNull()

        fun parse(uri: Uri?): PairingOffer? {
            if (uri == null || uri.host != HOST || uri.path != PATH) return null
            val fields = (uri.fragment ?: return null).split('&').mapNotNull { pair ->
                val at = pair.indexOf('=')
                if (at <= 0) null else pair.substring(0, at) to pair.substring(at + 1)
            }.toMap()
            if (fields["v"] != "1") return null
            val port = fields["p"]?.toIntOrNull() ?: return null
            val fingerprint = fields["f"]?.takeIf { it.length == 64 } ?: return null
            val token = fields["t"]?.takeIf { it.length == 32 } ?: return null
            val addresses = fields["a"]?.split(',')?.filter { it.isNotBlank() } ?: return null
            if (addresses.isEmpty()) return null
            return PairingOffer(
                pcName = URLDecoder.decode(fields["n"] ?: "PC", "UTF-8"),
                addresses = addresses,
                port = port,
                fingerprint = fingerprint,
                token = token,
            )
        }
    }
}
