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
#include "BackgroundService.h"
#ifdef WIN32
    #include <Windows.h>
    #include <shellapi.h>
#elif _UNIX
    #include <wx/wxprec.h>
    #ifndef WX_PRECOMP
        #include <wx/wx.h>
    #endif
    #include <wx/utils.h>
    #ifdef _MACOS
        #include <unistd.h>
    #endif
#endif

using std::wstring;

namespace {
#ifndef _MACOS
constexpr wchar_t serviceName[] = L"backgroundsync";  // [BackgroundService.md#serviceName]
#endif

#ifdef WIN32
constexpr DWORD elevationTimeout = 30000;  // [BackgroundService.md#elevationTimeout]

class ScHandle {  // CloseServiceHandle is not CloseHandle, so util::AutoHandle does not fit
    SC_HANDLE handle_;
public:
    explicit ScHandle(SC_HANDLE handle): handle_(handle) {}
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
    ~ScHandle() { if (handle_ != nullptr) CloseServiceHandle(handle_); }
    operator SC_HANDLE() const { return handle_; }
};

wstring systemMessage(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    const wstring text = length > 0 ? wstring(buffer, length) : L"Fehler " + std::to_wstring(error);
    LocalFree(buffer);
    return text;
}

wstring failure(const wstring& action, DWORD error)
{
    if (error == ERROR_SERVICE_DOES_NOT_EXIST)
        return L"Der Hintergrunddienst ist nicht installiert. Bitte zuerst install.ps1 des Projektes backgroundsync ausfuehren.";
    return L"Der Hintergrunddienst konnte nicht " + action + L" werden:\n" + systemMessage(error);
}

wstring elevate(const wstring& verb, const wstring& action)  // [BackgroundService.md#elevate]
{
    const wstring parameters = verb + L" " + serviceName;
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    info.lpFile = L"sc.exe";
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        if (error == ERROR_CANCELLED)
            return L"Der Hintergrunddienst konnte nicht " + action + L" werden: die Administratorrechte wurden abgelehnt.";
        return L"Der Hintergrunddienst konnte nicht " + action + L" werden: sc.exe liess sich nicht starten:\n" + systemMessage(error);
    }
    if (info.hProcess == nullptr)
        return L"";
    WaitForSingleObject(info.hProcess, elevationTimeout);  // [BackgroundService.md#sc-exit-code]
    DWORD exitCode = 0;
    const bool known = GetExitCodeProcess(info.hProcess, &exitCode) != FALSE;
    CloseHandle(info.hProcess);
    if (!known || exitCode == 0 || exitCode == STILL_ACTIVE)
        return L"";
    return failure(action, exitCode);
}

wstring withService(DWORD access, const wstring& verb, const wstring& action, bool (*control)(SC_HANDLE))  // [BackgroundService.md#withService]
{
    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (manager == nullptr)
        return failure(action, GetLastError());
    ScHandle service(OpenServiceW(manager, serviceName, access));
    if (service == nullptr)
        return GetLastError() == ERROR_ACCESS_DENIED ? elevate(verb, action) : failure(action, GetLastError());
    if (control(service))
        return L"";
    return GetLastError() == ERROR_ACCESS_DENIED ? elevate(verb, action) : failure(action, GetLastError());
}

BackgroundService::State queryState()  // [BackgroundService.md#queryState-access]
{
    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (manager == nullptr)
        return BackgroundService::State::Unknown;
    ScHandle service(OpenServiceW(manager, serviceName, SERVICE_QUERY_STATUS));
    if (service == nullptr)
        return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? BackgroundService::State::NotInstalled : BackgroundService::State::Unknown;
    SERVICE_STATUS status{};
    if (!QueryServiceStatus(service, &status))
        return BackgroundService::State::Unknown;
    return status.dwCurrentState == SERVICE_STOPPED ? BackgroundService::State::Stopped : BackgroundService::State::Running;
}
#elif _MACOS
constexpr wchar_t launchAgentLabel[] = L"com.autofilesync.backgroundsync";  // [BackgroundService.md#launchAgentLabel]

wxString agentPlistPath()
{
    return wxGetHomeDir() + "/Library/LaunchAgents/" + wxString(launchAgentLabel) + ".plist";
}

wxString domainTarget() { return wxString::Format("gui/%u", getuid()); }
wxString serviceTarget() { return domainTarget() + "/" + wxString(launchAgentLabel); }

struct LaunchctlResult { long status = -1; wxArrayString output, errors; };

LaunchctlResult runLaunchctl(const wxString& arguments)  // [BackgroundService.md#runLaunchctl]
{
    LaunchctlResult result;
    result.status = wxExecute("launchctl " + arguments, result.output, result.errors, wxEXEC_SYNC | wxEXEC_NOEVENTS);
    return result;
}

LaunchctlResult listAgent() { return runLaunchctl("list " + wxString(launchAgentLabel)); }

wstring launchctlFailure(const wstring& action, const LaunchctlResult& result)
{
    wxString reason;
    for (const wxString& line : result.errors)
        reason << line << "\n";
    if (reason.empty())
        reason = wxString::Format("launchctl endete mit %ld.", result.status);
    return L"Der Hintergrunddienst konnte nicht " + action + L" werden:\n" + reason.ToStdWstring();
}

wstring notInstalled()
{
    return L"Der Hintergrunddienst ist nicht installiert. Es fehlt der LaunchAgent "
        + agentPlistPath().ToStdWstring() + L".";
}

BackgroundService::State queryState()  // [BackgroundService.md#launchctl-list-pid]
{
    if (!wxFileExists(agentPlistPath()))
        return BackgroundService::State::NotInstalled;
    const LaunchctlResult listed = listAgent();
    if (listed.status != 0)
        return BackgroundService::State::Stopped;
    for (const wxString& line : listed.output)
        if (line.Contains("\"PID\""))
            return BackgroundService::State::Running;
    return BackgroundService::State::Stopped;
}

wstring startAgent()  // [BackgroundService.md#startAgent]
{
    if (!wxFileExists(agentPlistPath()))
        return notInstalled();
    if (listAgent().status != 0) {
        const LaunchctlResult bootstrapped = runLaunchctl("bootstrap " + domainTarget() + " \"" + agentPlistPath() + "\"");
        if (bootstrapped.status != 0)
            return launchctlFailure(L"gestartet", bootstrapped);
    }
    const LaunchctlResult started = runLaunchctl("kickstart " + serviceTarget());
    return started.status == 0 ? L"" : launchctlFailure(L"gestartet", started);
}

bool waitUntilUnregistered()  // [BackgroundService.md#waitUntilUnregistered]
{
    constexpr int attempts = 50, pollInterval = 100; // 5 s, ample for one pass to end
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (listAgent().status != 0)
            return true;
        wxMilliSleep(pollInterval);
    }
    return false;
}

wstring stopAgent()  // [BackgroundService.md#stopAgent]
{
    if (listAgent().status != 0)
        return L""; // an agent that is not registered is already stopped
    const LaunchctlResult stopped = runLaunchctl("bootout " + serviceTarget());
    if (stopped.status != 0)
        return launchctlFailure(L"gestoppt", stopped);
    if (!waitUntilUnregistered())
        return L"Der Hintergrunddienst wurde zum Beenden aufgefordert, läuft aber noch.";
    return L"";
}
#elif _UNIX
wstring runSystemctl(const wstring& verb, const wstring& action)  // [BackgroundService.md#runSystemctl]
{
    wxArrayString output, errors;
    const wxString command = wxString::Format("systemctl %s %s", wxString(verb), wxString(serviceName));
    if (wxExecute(command, output, errors, wxEXEC_SYNC) == 0)
        return L"";
    wxString reason;
    for (const wxString& line : errors)
        reason << line << "\n";
    if (reason.empty())
        reason = command;
    return L"Der Hintergrunddienst konnte nicht " + action + L" werden:\n" + reason.ToStdWstring();
}

BackgroundService::State queryState()  // [BackgroundService.md#queryState-systemctl]
{
    wxArrayString output, errors;
    const wxString command = wxString::Format("systemctl show %s -p LoadState -p ActiveState", wxString(serviceName));
    if (wxExecute(command, output, errors, wxEXEC_SYNC | wxEXEC_NOEVENTS) != 0)
        return BackgroundService::State::Unknown;
    BackgroundService::State result = BackgroundService::State::Stopped;
    for (const wxString& line : output) {
        if (line == "LoadState=not-found")
            return BackgroundService::State::NotInstalled;
        if (line.StartsWith("ActiveState=") && line != "ActiveState=inactive" && line != "ActiveState=failed")
            result = BackgroundService::State::Running;
    }
    return result;
}
#endif
}

namespace BackgroundService {

State state()
{
#if defined(WIN32) || defined(_UNIX)
    return queryState();
#else
    return State::NotInstalled;
#endif
}

wstring start()
{
#ifdef WIN32
    return withService(SERVICE_START, L"start", L"gestartet",  // an already running service is what the user asked for
        [](SC_HANDLE service) { return StartServiceW(service, 0, nullptr) != FALSE || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING; });
#elif _MACOS
    return startAgent();
#elif _UNIX
    return runSystemctl(L"start", L"gestartet");
#else
    return L"Der Hintergrunddienst wird auf dieser Plattform nicht unterstuetzt.";
#endif
}

wstring stop()
{
#ifdef WIN32
    return withService(SERVICE_STOP, L"stop", L"gestoppt",
        [](SC_HANDLE service) {
            SERVICE_STATUS status{};
            return ControlService(service, SERVICE_CONTROL_STOP, &status) != FALSE || GetLastError() == ERROR_SERVICE_NOT_ACTIVE;
        });
#elif _MACOS
    return stopAgent();
#elif _UNIX
    return runSystemctl(L"stop", L"gestoppt");
#else
    return L"Der Hintergrunddienst wird auf dieser Plattform nicht unterstuetzt.";
#endif
}

}
