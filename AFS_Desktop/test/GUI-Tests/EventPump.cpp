/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#include "EventPump.h"
#include <wx/app.h>
#include <wx/stopwatch.h>
#include <wx/utils.h>

namespace guitest {

void pump(int milliseconds)
{
    wxStopWatch watch;
    do {
        wxYield();
        wxMilliSleep(5);
    } while (watch.Time() < milliseconds);
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
