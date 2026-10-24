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
import android.util.Log
import org.apache.sshd.common.file.virtualfs.VirtualFileSystemFactory
import org.apache.sshd.common.keyprovider.KeyPairProvider
import org.apache.sshd.common.util.io.PathUtils
import org.apache.sshd.server.SshServer
import org.apache.sshd.server.session.ServerSession
import org.apache.sshd.sftp.server.FileHandle
import org.apache.sshd.sftp.server.Handle
import org.apache.sshd.sftp.server.SftpEventListener
import org.apache.sshd.sftp.server.SftpFileSystemAccessor
import org.apache.sshd.sftp.server.SftpSubsystemFactory
import org.apache.sshd.sftp.server.SftpSubsystemProxy
import java.io.IOException
import java.nio.file.CopyOption
import java.nio.file.LinkOption
import java.nio.file.Path
import java.nio.file.Paths
import java.nio.file.attribute.PosixFilePermissions
import java.security.SecureRandom
import java.util.Collections
import java.util.NavigableMap

class SftpServer(context: Context, private val hostKey: SshHostKey) {  // the phone's SFTP server, open only for a pass the PC runs (DeviceLink.md#serve in AFS_Core)
    private val filesDir = context.filesDir
    private var server: SshServer? = null
    private val changed = Collections.synchronizedSet(mutableSetOf<String>())

    class Access(val port: Int, val user: String, val password: String)

    fun start(): Access? {
        stop()
        changed.clear()
        val password = ByteArray(16).also { SecureRandom().nextBytes(it) }.joinToString("") { "%02x".format(it) }
        PathUtils.setUserHomeFolderResolver { filesDir.toPath() }  // MINA reads user.home, which Android leaves empty
        val sftp = SftpSubsystemFactory.Builder().withFileSystemAccessor(FixedPermissions).build()
        sftp.addSftpEventListener(ChangeRecorder())
        val sshd = SshServer.setUpDefaultServer().apply {
            port = PREFERRED_PORT
            keyPairProvider = KeyPairProvider.wrap(hostKey.keyPair)  // the key the PC pinned at pairing
            setPasswordAuthenticator { user, given, _ -> user == USER && given == password }
            subsystemFactories = listOf(sftp)
            fileSystemFactory = VirtualFileSystemFactory(Paths.get(ROOT))
        }
        return try {
            try {
                sshd.start()
            } catch (e: IOException) {
                sshd.port = 0  // the preferred port is taken: any free one, the hello names it
                sshd.start()
            }
            server = sshd
            Access(sshd.port, USER, password)
        } catch (e: Exception) {
            Log.w(TAG, "SFTP server did not start", e)
            null
        }
    }

    fun stop() {
        server?.stop(true)
        server = null
    }

    fun changedPaths(): List<String> = synchronized(changed) { changed.toList() }  // files the PC wrote, moved or removed, for the media scan

    private fun record(path: Path?) {
        val text = path?.toString() ?: return
        changed += if (text.startsWith(ROOT)) text else ROOT + text  // the virtual file system reports paths below its root
    }

    private inner class ChangeRecorder : SftpEventListener {
        override fun written(session: ServerSession, remoteHandle: String, localHandle: FileHandle, offset: Long, data: ByteArray, dataOffset: Int, dataLen: Int, thrown: Throwable?) = record(localHandle.file)
        override fun removed(session: ServerSession, path: Path, isDirectory: Boolean, thrown: Throwable?) = record(path)
        override fun moved(session: ServerSession, srcPath: Path, dstPath: Path, opts: Collection<CopyOption>, thrown: Throwable?) { record(srcPath); record(dstPath) }
        override fun closed(session: ServerSession, remoteHandle: String, localHandle: Handle, thrown: Throwable?) {}
    }

    private object FixedPermissions : SftpFileSystemAccessor {  // shared storage has no POSIX bits; report what a file on a PC would have
        override fun resolveReportedFileAttributes(subsystem: SftpSubsystemProxy, file: Path, flags: Int, attrs: NavigableMap<String, Any>, vararg options: LinkOption): NavigableMap<String, Any> {
            val directory = attrs["isDirectory"] == true
            attrs["permissions"] = PosixFilePermissions.fromString(if (directory) "rwxr-xr-x" else "rw-r--r--")
            return attrs
        }
    }

    companion object {
        private const val TAG = "AFS-Sftp"
        const val PREFERRED_PORT = 52382
        const val USER = "afs"
        const val ROOT = "/storage/emulated/0"
    }
}
