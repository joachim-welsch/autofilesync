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
#include "UsbPhone.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef AFS_ANDROID_RES_DIR
#define AFS_ANDROID_RES_DIR ""  // set by CMake, GuiTests.vcxproj and run_GUI_Tests.ps1; check() skips the test without it
#endif

namespace guitest {
namespace {

using Command = UsbPhone::Command;

constexpr const char* PACKAGE = "com.welschsoftware.autofilesync";
constexpr const char* PAIRING_PREFS = "shared_prefs/pc.xml";  // PcStore in the app [UsbPhone.md#pairing-backup]
constexpr const char* UI_DUMP = "/sdcard/afs-guitest-ui.xml";

struct Output {
    int status = -1;
    std::string text;
};

std::string shell_quoted(const std::string& text)  // [UsbPhone.md#quoting]
{
    std::string result = "'";
    for (const char c : text)
        result += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return result + "'";
}

std::string utf8_of(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

std::string adb_program()  // [UsbPhone.md#adb_program]
{
    std::vector<std::filesystem::path> sdks;
    for (const char* variable : {"ANDROID_HOME", "ANDROID_SDK_ROOT"})
        if (const char* dir = std::getenv(variable))
            sdks.emplace_back(dir);
#ifdef _WIN32
    if (const char* local = std::getenv("LOCALAPPDATA"))
        sdks.push_back(std::filesystem::path(local) / "Android" / "Sdk");
    constexpr const char* ADB_FILE = "adb.exe";
#else
    if (const char* home = std::getenv("HOME")) {
        sdks.push_back(std::filesystem::path(home) / "Android" / "Sdk");
        sdks.push_back(std::filesystem::path(home) / "Library" / "Android" / "sdk");
    }
    constexpr const char* ADB_FILE = "adb";
#endif
    for (const auto& sdk : sdks)
        if (std::error_code error; std::filesystem::is_regular_file(sdk / "platform-tools" / ADB_FILE, error))
            return utf8_of(sdk / "platform-tools" / ADB_FILE);
    return "adb";
}

#ifndef _WIN32

std::string command_line(const Command& command)
{
    std::string line;
    for (const auto& argument : command)
        line += (line.empty() ? "" : " ") + shell_quoted(argument);
    return line;
}

Output run(const Command& command)
{
    Output output;
    FILE* pipe = popen((command_line(command) + " 2>&1").c_str(), "r");
    if (!pipe)
        return output;
    std::array<char, 4096> buffer;
    while (const size_t n = fread(buffer.data(), 1, buffer.size(), pipe))
        output.text.append(buffer.data(), n);
    const int status = pclose(pipe);
    output.status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return output;
}

bool run_with_input(const Command& command, const std::string& input)
{
    FILE* pipe = popen((command_line(command) + " >/dev/null 2>&1").c_str(), "w");
    if (!pipe)
        return false;
    const bool written = fwrite(input.data(), 1, input.size(), pipe) == input.size();
    return pclose(pipe) == 0 && written;
}

#else

struct HandleCloser {
    void operator()(HANDLE handle) const
    {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};
using Handle = std::unique_ptr<void, HandleCloser>;

struct Pipe {
    Handle read;
    Handle write;
};

std::wstring wide(const std::string& text)
{
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string argv_quoted(const std::string& argument)  // [UsbPhone.md#windows-quoting]
{
    if (!argument.empty() && argument.find_first_of(" \t\"") == std::string::npos)
        return argument;
    std::string quoted = "\"";
    size_t backslashes = 0;
    for (const char c : argument) {
        if (c == '\\') {
            ++backslashes;
            continue;
        }
        quoted.append(c == '"' ? 2 * backslashes + 1 : backslashes, '\\');
        quoted += c;
        backslashes = 0;
    }
    return quoted.append(2 * backslashes, '\\') + "\"";
}

Handle null_device()
{
    SECURITY_ATTRIBUTES inherited{sizeof(inherited), nullptr, TRUE};
    return Handle(CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited, OPEN_EXISTING, 0, nullptr));
}

std::optional<Pipe> child_pipe(bool child_reads)
{
    SECURITY_ATTRIBUTES inherited{sizeof(inherited), nullptr, TRUE};
    HANDLE read = nullptr;
    HANDLE write = nullptr;
    if (!CreatePipe(&read, &write, &inherited, 0))
        return std::nullopt;
    Pipe pipe{Handle(read), Handle(write)};
    SetHandleInformation(child_reads ? write : read, HANDLE_FLAG_INHERIT, 0);  // our end must not reach the child, or its end of file never comes
    return pipe;
}

Handle start(const Command& command, HANDLE input, HANDLE output)
{
    std::string line;
    for (const auto& argument : command)
        line += (line.empty() ? "" : " ") + argv_quoted(argument);
    std::wstring wide_line = wide(line);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = output;
    startup.hStdError = output;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, wide_line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        return Handle(INVALID_HANDLE_VALUE);
    CloseHandle(process.hThread);
    return Handle(process.hProcess);
}

int exit_code(const Handle& process)
{
    WaitForSingleObject(process.get(), INFINITE);
    DWORD code = 0;
    return GetExitCodeProcess(process.get(), &code) ? static_cast<int>(code) : -1;
}

Output run(const Command& command)
{
    Output output;
    auto pipe = child_pipe(false);
    const Handle nothing = null_device();
    if (!pipe)
        return output;
    const Handle process = start(command, nothing.get(), pipe->write.get());
    pipe->write.reset();  // the read below ends only when the child holds the last write end
    if (process.get() == INVALID_HANDLE_VALUE)
        return output;
    std::array<char, 4096> buffer;
    for (DWORD n = 0; ReadFile(pipe->read.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &n, nullptr) && n > 0;)
        output.text.append(buffer.data(), n);
    output.status = exit_code(process);
    return output;
}

bool run_with_input(const Command& command, const std::string& input)
{
    auto pipe = child_pipe(true);
    const Handle nothing = null_device();
    if (!pipe)
        return false;
    const Handle process = start(command, pipe->read.get(), nothing.get());
    pipe->read.reset();
    if (process.get() == INVALID_HANDLE_VALUE)
        return false;
    DWORD n = 0;
    const bool written = WriteFile(pipe->write.get(), input.data(), static_cast<DWORD>(input.size()), &n, nullptr) && n == input.size();
    pipe->write.reset();  // end of file for the child's stdin
    return exit_code(process) == 0 && written;
}

#endif

std::string unescaped(std::string text)
{
    for (const auto& [from, to] : {std::pair{"\\'", "'"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"}})
        for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + std::string(to).size()))
            text.replace(at, std::string(from).size(), to);
    return text;
}

std::vector<std::string> translations_of(const std::string& string_name)  // [UsbPhone.md#translations_of]
{
    std::vector<std::string> texts;
    const std::regex entry("<string name=\"" + string_name + "\">([^<]*)</string>");
    for (const auto& dir : std::filesystem::directory_iterator(AFS_ANDROID_RES_DIR)) {
        std::ifstream in(dir.path() / "strings.xml");
        const std::string xml((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (std::smatch match; std::regex_search(xml, match, entry))
            texts.push_back(unescaped(match[1]));
    }
    return texts;
}

std::regex pattern_of(const std::string& text)  // [UsbPhone.md#shows]
{
    static const std::regex special(R"re([.^$|()\[\]{}*+?\\])re");
    static const std::regex placeholder(R"re(%\d\\\$[sd])re");
    return std::regex(std::regex_replace(std::regex_replace(text, special, "\\$&"), placeholder, ".*"));
}

std::vector<std::string> texts_of(const std::string& ui)
{
    static const std::regex node(R"re(<node [^>]*?text="([^"]*)")re");
    std::vector<std::string> texts;
    for (std::sregex_iterator it(ui.begin(), ui.end(), node), end; it != end; ++it)
        texts.push_back(unescaped((*it)[1]));
    return texts;
}

std::optional<std::pair<int, int>> centre_of(const std::string& ui, const std::vector<std::string>& texts)
{
    static const std::regex node(R"re(<node [^>]*?text="([^"]*)"[^>]*?bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]")re");
    for (std::sregex_iterator it(ui.begin(), ui.end(), node), end; it != end; ++it)
        if (std::ranges::find(texts, unescaped((*it)[1])) != texts.end())
            return std::pair{(std::stoi((*it)[2]) + std::stoi((*it)[4])) / 2, (std::stoi((*it)[3]) + std::stoi((*it)[5])) / 2};
    return std::nullopt;
}

}

std::string app_package()
{
    return PACKAGE;
}

UsbPhone::UsbPhone()
{
    why_not_ = check();
    if (ready() && !backup_pairing())
        why_not_ = "die Kopplung der App ließ sich nicht sichern";
}

UsbPhone::~UsbPhone()
{
    if (serial_.empty())
        return;
    for (auto it = undo_.rbegin(); it != undo_.rend(); ++it)
        run(*it);
    shell(std::string("rm -f ") + UI_DUMP);
    if (backed_up_)
        restore_pairing();
}

std::string UsbPhone::check()  // [UsbPhone.md#check]
{
    if (std::error_code error; !std::filesystem::is_directory(AFS_ANDROID_RES_DIR, error))
        return "die Texte der App fehlen (AFS_ANDROID_RES_DIR)";
    const Output devices = run({adb_program(), "devices"});
    if (devices.status != 0)
        return "adb ist nicht installiert (Android SDK platform-tools)";
    std::istringstream lines(devices.text);
    std::vector<std::string> ready_serials;
    for (std::string line; std::getline(lines, line);) {
        if (line.find("\tunauthorized") != std::string::npos)
            return "USB-Debugging ist am Handy noch nicht erlaubt (Dialog auf dem Handy bestätigen)";
        if (const auto tab = line.find("\tdevice"); tab != std::string::npos)
            ready_serials.push_back(line.substr(0, tab));
    }
    if (ready_serials.size() != 1)
        return ready_serials.empty() ? "kein Handy mit USB-Debugging angeschlossen" : "mehr als ein Handy angeschlossen";
    serial_ = ready_serials.front();
    if (shell(std::string("pm path ") + PACKAGE).find("package:") == std::string::npos)
        return "die App AutoFileSync ist auf dem Handy nicht installiert";
    int status = -1;
    shell(std::string("run-as ") + PACKAGE + " true", &status);
    if (status != 0)
        return "die App ist kein Debug-Build (run-as geht nicht, die Kopplung ließe sich nicht wiederherstellen)";
    if (shell(std::string("appops get ") + PACKAGE + " MANAGE_EXTERNAL_STORAGE").find("allow") == std::string::npos)
        return "der App fehlt die Freigabe \"Zugriff auf alle Dateien\"";
    if (!unlocked())
        return "das Handy ist gesperrt (entsperren und den Test wiederholen)";
    return {};
}

std::string UsbPhone::shell(const std::string& phone_command, int* status)
{
    const Output output = run(adb({"shell", phone_command}));
    if (status)
        *status = output.status;
    return output.text;
}

UsbPhone::Command UsbPhone::adb(Command arguments) const  // [UsbPhone.md#quoting]
{
    static const std::string program = adb_program();
    arguments.insert(arguments.begin(), {program, "-s", serial_});
    return arguments;
}

bool UsbPhone::backup_pairing()
{
    int status = -1;
    shell(std::string("run-as ") + PACKAGE + " ls " + PAIRING_PREFS, &status);
    if (status == 0) {
        const Output prefs = run(adb({"exec-out", std::string("run-as ") + PACKAGE + " cat " + PAIRING_PREFS}));
        if (prefs.status != 0 || prefs.text.find("<map") == std::string::npos)
            return false;
        pairing_ = prefs.text;
    }
    backed_up_ = true;
    return true;
}

bool UsbPhone::restore_pairing()  // [UsbPhone.md#pairing-backup]
{
    shell(std::string("am force-stop ") + PACKAGE);
    if (!pairing_) {
        int status = -1;
        shell(std::string("run-as ") + PACKAGE + " rm -f " + PAIRING_PREFS, &status);
        return status == 0;
    }
    return run_with_input(adb({"exec-in", std::string("run-as ") + PACKAGE + " sh -c " + shell_quoted(std::string("cat > ") + PAIRING_PREFS)}), *pairing_);
}

bool UsbPhone::reverse(uint16_t port)  // [UsbPhone.md#reverse]
{
    const std::string spec = "tcp:" + std::to_string(port);
    if (run(adb({"reverse", spec, spec})).status != 0)
        return false;
    undo_.push_back(adb({"reverse", "--remove", spec}));
    return true;
}

bool UsbPhone::forward(uint16_t port)  // [UsbPhone.md#forward]
{
    const std::string spec = "tcp:" + std::to_string(port);
    if (run(adb({"forward", spec, spec})).status != 0)
        return false;
    undo_.push_back(adb({"forward", "--remove", spec}));
    return true;
}

bool UsbPhone::unlocked()  // [UsbPhone.md#unlocked]
{
    shell("input keyevent KEYCODE_WAKEUP");
    shell("wm dismiss-keyguard");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return shell("dumpsys window").find("isKeyguardShowing=true") == std::string::npos;
}

bool UsbPhone::open_link(const std::string& url)  // [UsbPhone.md#open_link]
{
    unlocked();
    int status = -1;
    const std::string text = shell(std::string("am start -W -a android.intent.action.VIEW -p ") + PACKAGE + " -d " + shell_quoted(url), &status);
    return status == 0 && text.find("Error") == std::string::npos;
}

std::string UsbPhone::screen_texts()
{
    shell(std::string("uiautomator dump ") + UI_DUMP);
    return run(adb({"exec-out", std::string("cat ") + UI_DUMP})).text;
}

bool UsbPhone::shows(const std::string& string_name)  // [UsbPhone.md#shows]
{
    const auto texts = texts_of(screen_texts());
    for (const auto& translation : translations_of(string_name)) {
        const std::regex wanted = pattern_of(translation);
        if (std::ranges::any_of(texts, [&](const std::string& text) { return std::regex_match(text, wanted); }))
            return true;
    }
    return false;
}

bool UsbPhone::shows_text(const std::string& text)
{
    const auto texts = texts_of(screen_texts());
    return std::ranges::find(texts, text) != texts.end();
}

bool UsbPhone::start_pass()  // [UsbPhone.md#pass]
{
    run(adb({"logcat", "-c"}));
    return tap("sync_now", std::chrono::seconds(20));
}

bool UsbPhone::pass_finished()
{
    const std::string log = run(adb({"logcat", "-d", "-s", "WM-WorkerWrapper:I"})).text;
    return log.find("Worker result") != std::string::npos && log.find("SyncWorker") != std::string::npos;
}

bool UsbPhone::tap(const std::string& string_name, std::chrono::seconds timeout)  // [UsbPhone.md#tap]
{
    const auto texts = translations_of(string_name);
    for (const auto end = std::chrono::steady_clock::now() + timeout; std::chrono::steady_clock::now() < end; std::this_thread::sleep_for(std::chrono::milliseconds(500))) {
        if (const auto centre = centre_of(screen_texts(), texts)) {
            shell("input tap " + std::to_string(centre->first) + " " + std::to_string(centre->second));
            return true;
        }
    }
    return false;
}

bool UsbPhone::write_file(const std::string& path, const std::string& content)
{
    const std::string dir = std::filesystem::path(path).parent_path().generic_string();
    return run_with_input(adb({"exec-in", "mkdir -p " + shell_quoted(dir) + " && cat > " + shell_quoted(path)}), content);
}

std::optional<std::string> UsbPhone::read_file(const std::string& path)
{
    int status = -1;
    shell("test -f " + shell_quoted(path), &status);
    if (status != 0)
        return std::nullopt;
    return run(adb({"exec-out", "cat " + shell_quoted(path)})).text;
}

std::vector<std::string> UsbPhone::list_dir(const std::string& path)
{
    int status = -1;
    std::istringstream lines(shell("ls -1 " + shell_quoted(path), &status));
    std::vector<std::string> names;
    for (std::string line; status == 0 && std::getline(lines, line);) {
        if (line.ends_with('\r'))  // adb on Windows may end lines with CRLF
            line.pop_back();
        if (!line.empty())
            names.push_back(line);
    }
    std::ranges::sort(names);
    return names;
}

bool UsbPhone::remove_file(const std::string& path)
{
    int status = -1;
    shell("rm " + shell_quoted(path), &status);
    return status == 0;
}

bool UsbPhone::move_file(const std::string& from, const std::string& to)
{
    int status = -1;
    shell("mv " + shell_quoted(from) + " " + shell_quoted(to), &status);
    return status == 0;
}

void UsbPhone::remove_dir(const std::string& path)
{
    shell("rm -rf " + shell_quoted(path));
}

}
