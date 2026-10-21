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

import java.io.File
import java.nio.file.Files
import java.nio.file.attribute.FileTime
import java.nio.file.attribute.PosixFilePermissions
import java.util.concurrent.TimeUnit

class MtimeProbe(private val report: (String) -> Unit)
{
    private val root = File("/storage/emulated/0")
    private val dirs = listOf("AFS-Spike", "DCIM/AFS-Spike", "Download/AFS-Spike", "Music/AFS-Spike", "Pictures/AFS-Spike")
    private val target = (System.currentTimeMillis() / 1000 - TimeUnit.DAYS.toSeconds(3)) * 1000

    fun run()
    {
        report("mtime target ${target / 1000}")
        dirs.forEach { probe_dir(File(root, it)) }
    }

    private fun probe_dir(dir: File)
    {
        val created = dir.mkdirs()
        if (!dir.isDirectory) return report("FAIL ${dir}: mkdir")
        try {
            probe_file(dir)
            probe_dir_time(dir)
        } catch (e: Exception) {
            report("FAIL ${dir}: ${e}")
        } finally {
            dir.listFiles()?.forEach { it.delete() }
            if (created) dir.delete()
        }
    }

    private fun probe_file(dir: File)
    {
        val file = File(dir, "probe.bin").apply { writeBytes(ByteArray(4096) { it.toByte() }) }
        val set_by_file = file.setLastModified(target)
        report(line(dir, "File.setLastModified", set_by_file, file.lastModified()))
        file.writeBytes(ByteArray(10))
        val set_by_nio = runCatching { Files.setLastModifiedTime(file.toPath(), FileTime.fromMillis(target)) }
        report(line(dir, "Files.setLastModifiedTime", set_by_nio.isSuccess, file.lastModified(), set_by_nio.exceptionOrNull()))
        val renamed = File(dir, "probe-renamed.bin")
        report(line(dir, "after rename", file.renameTo(renamed), renamed.lastModified()))
        val chmod = runCatching { Files.setPosixFilePermissions(renamed.toPath(), PosixFilePermissions.fromString("rw-r-----")) }
        val perms = runCatching { PosixFilePermissions.toString(Files.getPosixFilePermissions(renamed.toPath())) }
        report("${dir}: chmod 640 ok=${chmod.isSuccess} ${chmod.exceptionOrNull() ?: ""} now=${perms.getOrElse { it.toString() }}")
    }

    private fun probe_dir_time(dir: File)
    {
        val sub = File(dir, "sub").apply { mkdir() }
        report(line(dir, "dir setLastModified", sub.setLastModified(target), sub.lastModified()))
        sub.delete()
    }

    private fun line(dir: File, what: String, ok: Boolean, actual: Long, error: Throwable? = null): String
    {
        val verdict = if (actual / 1000 == target / 1000) "OK" else "MISMATCH"
        return "${dir}: ${what} returned=${ok} read=${actual / 1000} ${verdict} ${error ?: ""}"
    }
}
