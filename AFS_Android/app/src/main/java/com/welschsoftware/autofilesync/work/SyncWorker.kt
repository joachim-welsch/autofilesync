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
package com.welschsoftware.autofilesync.work

import android.content.Context
import android.content.pm.ServiceInfo
import android.os.BatteryManager
import android.os.Build
import androidx.work.CoroutineWorker
import androidx.work.ForegroundInfo
import androidx.work.WorkerParameters
import com.welschsoftware.autofilesync.link.PcStore
import java.util.concurrent.TimeUnit

class SyncWorker(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {  // one pass in the background: the server opens, the PC synchronises, the server closes
    override suspend fun doWork(): Result {
        val store = PcStore(applicationContext)
        if (store.pc == null) return Result.success()
        if (inputData.getBoolean(Scheduler.KEY_CONTENT, false))
            Scheduler.watchMedia(applicationContext)  // a content trigger fires once; the next one is armed right away
        if (!batteryAllows()) return Result.success()
        setForeground(getForegroundInfo())
        val state = Pass.run(applicationContext, serve = true) { done, total -> setForegroundAsync(foregroundInfo(done, total)) }
        if (state.outcome == Outcome.UNREACHABLE) notifyWhenLongUnreachable(store)
        return Result.success()
    }

    override suspend fun getForegroundInfo(): ForegroundInfo = foregroundInfo(0, 0)

    private fun foregroundInfo(done: Int, total: Int): ForegroundInfo {
        val notification = Notifications.progress(applicationContext, done, total)
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
            ForegroundInfo(Notifications.PROGRESS_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        else
            ForegroundInfo(Notifications.PROGRESS_ID, notification)
    }

    private fun batteryAllows(): Boolean {  // a nearly empty phone is not the moment for a large backup
        val battery = applicationContext.getSystemService(BatteryManager::class.java)
        return battery.isCharging || battery.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY) >= LOW_BATTERY_PERCENT
    }

    private fun notifyWhenLongUnreachable(store: PcStore) {
        val now = System.currentTimeMillis()
        val day = TimeUnit.DAYS.toMillis(1)
        if (store.lastContact != 0L && now - store.lastContact >= day && now - store.unreachableNotified >= day) {
            Notifications.unreachable(applicationContext)
            store.unreachableNotified = now
        }
    }

    companion object {
        private const val LOW_BATTERY_PERCENT = 15
    }
}
