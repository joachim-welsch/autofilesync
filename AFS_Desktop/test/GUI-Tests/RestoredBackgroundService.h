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
#pragma once

#include "BackgroundService.h"
#include <string>
#ifdef _WIN32
    #include <windows.h>
#elif !defined(__APPLE__)
    #include <wx/arrstr.h>
    #include <wx/utils.h>
#endif

namespace guitest {

inline bool service_controllable()  // [RestoredBackgroundService.md#service_controllable]
{
    using State = BackgroundService::State;
    const State state = BackgroundService::state();
    if (state == State::NotInstalled || state == State::Unknown)
        return false;
#ifdef _WIN32
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    SC_HANDLE service = manager != nullptr ? OpenServiceW(manager, L"backgroundsync", SERVICE_START | SERVICE_STOP) : nullptr;
    if (service != nullptr)
        CloseServiceHandle(service);
    if (manager != nullptr)
        CloseServiceHandle(manager);
    return service != nullptr;
#elif defined(__APPLE__)
    return true;  // a LaunchAgent belongs to the user
#else
    wxArrayString output, errors;
    const wxString unchanged = state == State::Running ? "start" : "stop";
    return wxExecute("systemctl --no-ask-password " + unchanged + " backgroundsync.service", output, errors, wxEXEC_SYNC) == 0;
#endif
}

inline std::wstring start_obstacle()  // [RestoredBackgroundService.md#start_obstacle]
{
    if (BackgroundService::state() == BackgroundService::State::Running)
        return L"";
    return BackgroundService::start();
}

class RestoredBackgroundService {  // [RestoredBackgroundService.md#RestoredBackgroundService]
public:
    RestoredBackgroundService(): was_running_(running()) {}
    ~RestoredBackgroundService()
    {
        if (running() && !was_running_)
            BackgroundService::stop();
        else if (!running() && was_running_)
            BackgroundService::start();
    }
    RestoredBackgroundService(const RestoredBackgroundService&) = delete;
    RestoredBackgroundService& operator=(const RestoredBackgroundService&) = delete;
private:
    static bool running() { return BackgroundService::state() == BackgroundService::State::Running; }
    bool was_running_;
};

}
