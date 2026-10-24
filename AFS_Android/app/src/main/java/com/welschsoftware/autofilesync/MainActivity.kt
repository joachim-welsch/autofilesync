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
package com.welschsoftware.autofilesync

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.material3.MaterialTheme
import com.welschsoftware.autofilesync.ui.AppRoot
import com.welschsoftware.autofilesync.work.Notifications
import com.welschsoftware.autofilesync.work.Scheduler

class MainActivity : ComponentActivity() {
    private val state: AppState by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { MaterialTheme { AppRoot(state) } }
        Notifications.createChannels(this)
        Scheduler.ensure(this)  // also after an update or a reboot that dropped the schedule
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED)
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), 0)  // the progress of a background pass is shown as a notification
        if (savedInstanceState == null) state.onLink(intent?.data)  // a scanned link from the camera app opened the app
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        state.onLink(intent.data)
    }

    override fun onResume() {
        super.onResume()
        state.refreshAccess()  // the user may just have come back from the system's permission page
        if (state.screen == Screen.Status) state.reportIn()  // report in whenever the user looks, so the card is current
    }
}
