/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of backgroundsync, the background service of AutoFileSync.
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
#include <Windows.h>
#include <iostream>
#include <string>
#include "Logging.h"
#include "Translation.h"
#include "MyDaemon.h"

using namespace AFS_Core;
using translation::translate;

namespace {

constexpr wchar_t SERVICE_NAME[] = L"backgroundsync";  // [WindowsService.md#SERVICE_NAME]
constexpr DWORD STOP_WAIT_HINT = 120000;  // [WindowsService.md#STOP_WAIT_HINT]

SERVICE_STATUS_HANDLE status_handle = nullptr;
HANDLE stop_event = nullptr;

void report(DWORD state, DWORD wait_hint = 0)
{
    SERVICE_STATUS status{};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    status.dwWaitHint = wait_hint;
    SetServiceStatus(status_handle, &status);
}

void WINAPI control(DWORD code)
{
    if (code != SERVICE_CONTROL_STOP && code != SERVICE_CONTROL_SHUTDOWN)
        return;
    report(SERVICE_STOP_PENDING, STOP_WAIT_HINT);
    SetEvent(stop_event);
}

BOOL WINAPI console_control(DWORD)  // Ctrl+C and a closing console window end the console run
{
    SetEvent(stop_event);
    return TRUE;
}

void start_daemon(MyDaemon& daemon)
{
    daemon.start();
    log_info(translate("daemon_started"), L"");
}

void stop_daemon(MyDaemon& daemon)
{
    daemon.stop();
    log_info(translate("daemon_stopped"), L"");  // after stop(), so it is the daemon's last entry
}

void WINAPI service_main(DWORD, LPWSTR*)
{
    status_handle = RegisterServiceCtrlHandlerW(SERVICE_NAME, control);
    if (status_handle == nullptr)
        return;
    report(SERVICE_START_PENDING, STOP_WAIT_HINT);
    MyDaemon daemon;
    start_daemon(daemon);
    report(SERVICE_RUNNING);
    WaitForSingleObject(stop_event, INFINITE);
    stop_daemon(daemon);
    report(SERVICE_STOPPED);
}

int run_in_console()  // [WindowsService.md#run_in_console]
{
    SetConsoleOutputCP(CP_UTF8);  // the catalogues are UTF-8, the console's default code page is not
    std::cout << translation::translate_utf8("daemon_console_mode") << std::endl;
    SetConsoleCtrlHandler(console_control, TRUE);
    MyDaemon daemon;
    start_daemon(daemon);
    WaitForSingleObject(stop_event, INFINITE);
    stop_daemon(daemon);
    return 0;
}

}

int wmain()
{
    translation::init_locale(translation::DEFAULT_LOCALES);  // the service control manager names no language at all
    translation::init(translation::locale_dir());  // before the first message
    Logging::instance().origin(LogOrigin::Daemon);  // the GUI writes to the same file
    stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(SERVICE_NAME), service_main}, {nullptr, nullptr}};
    if (StartServiceCtrlDispatcherW(table))
        return 0;
    const DWORD error = GetLastError();
    if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)  // [WindowsService.md#run_in_console]
        return run_in_console();
    log_error(translate("daemon_dispatcher_failed", {std::to_wstring(error)}));
    return 1;
}
