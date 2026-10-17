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
#include "Translation.h"
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
        #include <wx/ffile.h>
        #include <wx/filename.h>
        #include <wx/stdpaths.h>
        #include <unistd.h>
    #endif
#endif

using std::wstring;
using AFS_Core::translation::translate;

namespace {
#ifndef _MACOS
constexpr wchar_t SERVICE_NAME[] = L"backgroundsync";  // [BackgroundService.md#SERVICE_NAME]
#endif

#ifdef WIN32
constexpr DWORD ELEVATION_TIMEOUT = 30000;  // [BackgroundService.md#ELEVATION_TIMEOUT]

class ScHandle {  // CloseServiceHandle is not CloseHandle, so util::AutoHandle does not fit
    SC_HANDLE handle_;
public:
    explicit ScHandle(SC_HANDLE handle): handle_(handle) {}
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
    ~ScHandle() { if (handle_ != nullptr) CloseServiceHandle(handle_); }
    operator SC_HANDLE() const { return handle_; }
};

wstring system_message(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    const wstring text = length > 0 ? wstring(buffer, length) : translate("bs_system_error", {std::to_wstring(error)});
    LocalFree(buffer);
    return text;
}

wstring failure(const wstring& action, DWORD error)
{
    if (error == ERROR_SERVICE_DOES_NOT_EXIST)
        return translate("bs_not_installed_windows");
    return translate("bs_action_failed", {action, system_message(error)});
}

wstring elevate(const wstring& verb, const wstring& action)  // [BackgroundService.md#elevate]
{
    const wstring parameters = verb + L" " + SERVICE_NAME;
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
            return translate("bs_elevation_denied", {action});
        return translate("bs_sc_start_failed", {action, system_message(error)});
    }
    if (info.hProcess == nullptr)
        return L"";
    WaitForSingleObject(info.hProcess, ELEVATION_TIMEOUT);  // [BackgroundService.md#sc-exit-code]
    DWORD exit_code = 0;
    const bool known = GetExitCodeProcess(info.hProcess, &exit_code) != FALSE;
    CloseHandle(info.hProcess);
    if (!known || exit_code == 0 || exit_code == STILL_ACTIVE)
        return L"";
    return failure(action, exit_code);
}

wstring with_service(DWORD access, const wstring& verb, const wstring& action, bool (*control)(SC_HANDLE))  // [BackgroundService.md#with_service]
{
    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (manager == nullptr)
        return failure(action, GetLastError());
    ScHandle service(OpenServiceW(manager, SERVICE_NAME, access));
    if (service == nullptr)
        return GetLastError() == ERROR_ACCESS_DENIED ? elevate(verb, action) : failure(action, GetLastError());
    if (control(service))
        return L"";
    return GetLastError() == ERROR_ACCESS_DENIED ? elevate(verb, action) : failure(action, GetLastError());
}

BackgroundService::State query_state()  // [BackgroundService.md#query_state-access]
{
    ScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (manager == nullptr)
        return BackgroundService::State::Unknown;
    ScHandle service(OpenServiceW(manager, SERVICE_NAME, SERVICE_QUERY_STATUS));
    if (service == nullptr)
        return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? BackgroundService::State::NotInstalled : BackgroundService::State::Unknown;
    SERVICE_STATUS status{};
    if (!QueryServiceStatus(service, &status))
        return BackgroundService::State::Unknown;
    return status.dwCurrentState == SERVICE_STOPPED ? BackgroundService::State::Stopped : BackgroundService::State::Running;
}
#elif _MACOS
constexpr wchar_t LAUNCH_AGENT_LABEL[] = L"com.autofilesync.backgroundsync";  // [BackgroundService.md#LAUNCH_AGENT_LABEL]

wxString agent_plist_path()
{
    return wxGetHomeDir() + "/Library/LaunchAgents/" + wxString(LAUNCH_AGENT_LABEL) + ".plist";
}

wxString domain_target() { return wxString::Format("gui/%u", getuid()); }
wxString service_target() { return domain_target() + "/" + wxString(LAUNCH_AGENT_LABEL); }

struct LaunchctlResult { long status = -1; wxArrayString output, errors; };

LaunchctlResult run_launchctl(const wxString& arguments)  // [BackgroundService.md#run_launchctl]
{
    LaunchctlResult result;
    result.status = wxExecute("launchctl " + arguments, result.output, result.errors, wxEXEC_SYNC | wxEXEC_NOEVENTS);
    return result;
}

LaunchctlResult list_agent() { return run_launchctl("list " + wxString(LAUNCH_AGENT_LABEL)); }

wstring launchctl_failure(const wstring& action, const LaunchctlResult& result)
{
    wxString reason;
    for (const wxString& line : result.errors)
        reason << line << "\n";
    if (reason.empty())
        reason = translate("bs_launchctl_exit", {std::to_wstring(result.status)});
    return translate("bs_action_failed", {action, reason.ToStdWstring()});
}

wxString daemon_path()  // [BackgroundService.md#daemon_path]
{
    wxFileName daemon(wxStandardPaths::Get().GetExecutablePath());
    daemon.SetFullName("backgroundsync");
    return daemon.GetFullPath();
}

wstring not_installed()
{
    return translate("bs_not_installed_macos", {daemon_path().ToStdWstring()});
}

wxString xml_escaped(wxString text)
{
    text.Replace("&", "&amp;");
    text.Replace("<", "&lt;");
    text.Replace(">", "&gt;");
    return text;
}

wxString agent_plist()  // [BackgroundService.md#agent_plist]
{
    return wxString::Format(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>Label</key><string>%s</string>\n"
        "    <key>ProgramArguments</key><array><string>%s</string></array>\n"
        "    <key>RunAtLoad</key><true/>\n"
        "    <key>KeepAlive</key><true/>\n"
        "</dict>\n"
        "</plist>\n", wxString(LAUNCH_AGENT_LABEL), xml_escaped(daemon_path()));
}

bool agent_plist_current()
{
    wxFFile file(agent_plist_path(), "r");
    wxString content;
    return file.IsOpened() && file.ReadAll(&content) && content == agent_plist();
}

wstring write_agent_plist()
{
    wxFileName::Mkdir(wxFileName(agent_plist_path()).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    wxFFile file(agent_plist_path(), "w");
    if (!file.IsOpened() || !file.Write(agent_plist()))
        return translate("bs_agent_not_writable", {agent_plist_path().ToStdWstring()});
    return L"";
}

bool wait_until_unregistered()  // [BackgroundService.md#wait_until_unregistered]
{
    constexpr int attempts = 50, poll_interval = 100; // 5 s, ample for one pass to end
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (list_agent().status != 0)
            return true;
        wxMilliSleep(poll_interval);
    }
    return false;
}

wstring unregister_agent()
{
    if (list_agent().status != 0)
        return L""; // an agent that is not registered is already stopped
    const LaunchctlResult stopped = run_launchctl("bootout " + service_target());
    if (stopped.status != 0)
        return launchctl_failure(translate("bs_action_stopped"), stopped);
    if (!wait_until_unregistered())
        return translate("bs_still_running");
    return L"";
}

BackgroundService::State query_state()  // [BackgroundService.md#launchctl-list-pid]
{
    if (!wxFileExists(daemon_path()))
        return BackgroundService::State::NotInstalled;
    const LaunchctlResult listed = list_agent();
    if (listed.status != 0)
        return BackgroundService::State::Stopped;
    for (const wxString& line : listed.output)
        if (line.Contains("\"PID\""))
            return BackgroundService::State::Running;
    return BackgroundService::State::Stopped;
}

wstring refresh_agent_plist()  // [BackgroundService.md#agent-follows-the-bundle]
{
    if (agent_plist_current())
        return L"";
    const wstring error = unregister_agent();
    return error.empty() ? write_agent_plist() : error;
}

wstring register_agent()
{
    if (list_agent().status == 0)
        return L"";
    const LaunchctlResult bootstrapped = run_launchctl("bootstrap " + domain_target() + " \"" + agent_plist_path() + "\"");
    return bootstrapped.status == 0 ? L"" : launchctl_failure(translate("bs_action_started"), bootstrapped);
}

wstring start_agent()  // [BackgroundService.md#start_agent]
{
    if (!wxFileExists(daemon_path()))
        return not_installed();
    for (wstring (*step)() : {refresh_agent_plist, register_agent}) {
        const wstring error = step();
        if (!error.empty())
            return error;
    }
    const LaunchctlResult started = run_launchctl("kickstart " + service_target());
    return started.status == 0 ? L"" : launchctl_failure(translate("bs_action_started"), started);
}

wstring stop_agent()  // [BackgroundService.md#stop_agent]
{
    const wstring error = unregister_agent();
    if (!error.empty())
        return error;
    if (wxFileExists(agent_plist_path()))
        wxRemoveFile(agent_plist_path());
    return L"";
}
#elif _UNIX
wstring run_systemctl(const wstring& verb, const wstring& action)  // [BackgroundService.md#run_systemctl]
{
    wxArrayString output, errors;
    const wxString command = wxString::Format("systemctl %s %s", wxString(verb), wxString(SERVICE_NAME));
    if (wxExecute(command, output, errors, wxEXEC_SYNC) == 0)
        return L"";
    wxString reason;
    for (const wxString& line : errors)
        reason << line << "\n";
    if (reason.empty())
        reason = command;
    return translate("bs_action_failed", {action, reason.ToStdWstring()});
}

BackgroundService::State query_state()  // [BackgroundService.md#query_state-systemctl]
{
    wxArrayString output, errors;
    const wxString command = wxString::Format("systemctl show %s -p LoadState -p ActiveState", wxString(SERVICE_NAME));
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
    return query_state();
#else
    return State::NotInstalled;
#endif
}

wstring start()
{
#ifdef WIN32
    return with_service(SERVICE_START, L"start", translate("bs_action_started"),  // an already running service is what the user asked for
        [](SC_HANDLE service) { return StartServiceW(service, 0, nullptr) != FALSE || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING; });
#elif _MACOS
    return start_agent();
#elif _UNIX
    return run_systemctl(L"start", translate("bs_action_started"));
#else
    return translate("bs_unsupported");
#endif
}

wstring stop()
{
#ifdef WIN32
    return with_service(SERVICE_STOP, L"stop", translate("bs_action_stopped"),
        [](SC_HANDLE service) {
            SERVICE_STATUS status{};
            return ControlService(service, SERVICE_CONTROL_STOP, &status) != FALSE || GetLastError() == ERROR_SERVICE_NOT_ACTIVE;
        });
#elif _MACOS
    return stop_agent();
#elif _UNIX
    return run_systemctl(L"stop", translate("bs_action_stopped"));
#else
    return translate("bs_unsupported");
#endif
}

}
