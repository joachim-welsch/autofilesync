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
import android.provider.MediaStore
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.OutOfQuotaPolicy
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.workDataOf
import com.welschsoftware.autofilesync.link.PcStore
import java.time.Duration

object Scheduler {  // when the phone reports in by itself: on a schedule, after new photos, and when the user asks
    const val KEY_CONTENT = "content"
    private const val PERIODIC = "periodic"
    private const val MEDIA = "media"
    private const val NOW = "now"
    private val INTERVAL: Duration = Duration.ofMinutes(30)
    private val MEDIA_SETTLE: Duration = Duration.ofMinutes(1)  // a burst of photos becomes one pass

    fun ensure(context: Context) {  // after pairing, at app start and when a setting changed
        val store = PcStore(context)
        val work = WorkManager.getInstance(context)
        if (store.pc == null) {
            work.cancelUniqueWork(PERIODIC)
            work.cancelUniqueWork(MEDIA)
            return
        }
        val periodic = PeriodicWorkRequestBuilder<SyncWorker>(INTERVAL).setConstraints(constraints(store)).build()
        work.enqueueUniquePeriodicWork(PERIODIC, ExistingPeriodicWorkPolicy.UPDATE, periodic)
        watchMedia(context)
    }

    fun watchMedia(context: Context) {
        val store = PcStore(context)
        val constraints = Constraints.Builder()
            .setRequiredNetworkType(if (store.wifiOnly) NetworkType.UNMETERED else NetworkType.CONNECTED)
            .setRequiresCharging(store.chargingOnly)
            .addContentUriTrigger(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, true)
            .addContentUriTrigger(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, true)
            .setTriggerContentUpdateDelay(MEDIA_SETTLE)
            .build()
        val request = OneTimeWorkRequestBuilder<SyncWorker>().setConstraints(constraints).setInputData(workDataOf(KEY_CONTENT to true)).build()
        WorkManager.getInstance(context).enqueueUniqueWork(MEDIA, ExistingWorkPolicy.REPLACE, request)
    }

    fun runNow(context: Context) {  // the button: at once, whatever the network setting says
        val request = OneTimeWorkRequestBuilder<SyncWorker>()
            .setConstraints(Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build())
            .setExpedited(OutOfQuotaPolicy.RUN_AS_NON_EXPEDITED_WORK_REQUEST)
            .build()
        WorkManager.getInstance(context).enqueueUniqueWork(NOW, ExistingWorkPolicy.KEEP, request)
    }

    private fun constraints(store: PcStore) = Constraints.Builder()
        .setRequiredNetworkType(if (store.wifiOnly) NetworkType.UNMETERED else NetworkType.CONNECTED)
        .setRequiresCharging(store.chargingOnly)
        .build()
}
