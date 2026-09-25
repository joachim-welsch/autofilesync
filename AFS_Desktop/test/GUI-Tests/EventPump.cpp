/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#include "EventPump.h"
#include <wx/app.h>
#include <wx/stopwatch.h>
#include <wx/utils.h>

namespace guitest {

constexpr int IDLE_STEP_MILLISECONDS = 5;

void pump(int milliseconds)  // [EventPump.md#idle-time-not-wall-time]
{
    for (int idle = 0; idle < milliseconds; idle += IDLE_STEP_MILLISECONDS) {
        wxYield();
        wxMilliSleep(IDLE_STEP_MILLISECONDS);
    }
    wxYield();
}

bool waitUntil(const std::function<bool()>& ready, int timeoutMilliseconds)
{
    wxStopWatch watch;
    while (!ready()) {
        if (watch.Time() > timeoutMilliseconds)
            return false;
        pump(10);
    }
    return true;
}

}
