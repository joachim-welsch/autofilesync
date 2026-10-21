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
package com.welschsoftware.autofilesync.spike

import org.apache.sshd.common.file.virtualfs.VirtualFileSystemFactory
import org.apache.sshd.common.util.io.PathUtils
import org.apache.sshd.server.SshServer
import org.apache.sshd.server.keyprovider.SimpleGeneratorHostKeyProvider
import org.apache.sshd.sftp.server.SftpSubsystemFactory
import java.io.File
import java.nio.file.Paths

class SpikeServer(private val files_dir: File)
{
    private var server: SshServer? = null

    fun start(port: Int): String
    {
        stop()
        PathUtils.setUserHomeFolderResolver { files_dir.toPath() }  // MINA's default reads user.home, which Android leaves empty
        val sshd = SshServer.setUpDefaultServer()
        sshd.port = port
        sshd.keyPairProvider = SimpleGeneratorHostKeyProvider(File(files_dir, "hostkey.ser").toPath()).apply { algorithm = "EC" }
        sshd.setPasswordAuthenticator { user, password, _ -> user == USER && password == PASSWORD }
        sshd.subsystemFactories = listOf(SftpSubsystemFactory())
        sshd.fileSystemFactory = VirtualFileSystemFactory(Paths.get(ROOT))
        sshd.start()
        server = sshd
        return "SFTP server on port ${port}, user ${USER}, root ${ROOT}"
    }

    fun stop()
    {
        server?.stop(true)
        server = null
    }

    companion object
    {
        const val USER = "afs"
        const val PASSWORD = "afs-spike"
        const val ROOT = "/storage/emulated/0"
    }
}
