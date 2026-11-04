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
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

#ifndef AFS_ANDROID_RES_DIR
#define AFS_ANDROID_RES_DIR ""  // set by CMake; the Windows build skips the test before reading it
#endif

namespace guitest {
namespace {

constexpr const char* PACKAGE = "com.welschsoftware.autofilesync";
constexpr const char* PAIRING_PREFS = "shared_prefs/pc.xml";  // PcStore in the app [UsbPhone.md#pairing-backup]
constexpr const char* UI_DUMP = "/sdcard/afs-guitest-ui.xml";

struct Output {
    int status = -1;
    std::string text;
};

#ifndef _WIN32

Output run(const std::string& command)
{
    Output output;
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (!pipe)
        return output;
    std::array<char, 4096> buffer;
    while (const size_t n = fread(buffer.data(), 1, buffer.size(), pipe))
        output.text.append(buffer.data(), n);
    const int status = pclose(pipe);
    output.status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return output;
}

bool run_with_input(const std::string& command, const std::string& input)
{
    FILE* pipe = popen((command + " >/dev/null 2>&1").c_str(), "w");
    if (!pipe)
        return false;
    const bool written = fwrite(input.data(), 1, input.size(), pipe) == input.size();
    return pclose(pipe) == 0 && written;
}

#else

Output run(const std::string&)
{
    return {};
}

bool run_with_input(const std::string&, const std::string&)
{
    return false;
}

#endif

std::string shell_quoted(const std::string& text)  // [UsbPhone.md#quoting]
{
    std::string result = "'";
    for (const char c : text)
        result += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return result + "'";
}

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
#ifdef _WIN32
    return "der Test steuert adb über eine POSIX-Shell (nur Linux und macOS)";
#else
    const Output devices = run("adb devices");
    if (devices.status != 0)
        return "adb ist nicht installiert";
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
#endif
}

std::string UsbPhone::shell(const std::string& phone_command, int* status)
{
    const Output output = run(adb("shell", phone_command));
    if (status)
        *status = output.status;
    return output.text;
}

std::string UsbPhone::adb(const std::string& arguments) const
{
    return "adb -s " + shell_quoted(serial_) + " " + arguments;
}

std::string UsbPhone::adb(const std::string& service, const std::string& phone_command) const  // [UsbPhone.md#quoting]
{
    return adb(service + " " + shell_quoted(phone_command));
}

bool UsbPhone::backup_pairing()
{
    int status = -1;
    shell(std::string("run-as ") + PACKAGE + " ls " + PAIRING_PREFS, &status);
    if (status == 0) {
        const Output prefs = run(adb("exec-out", std::string("run-as ") + PACKAGE + " cat " + PAIRING_PREFS));
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
    return run_with_input(adb("exec-in", std::string("run-as ") + PACKAGE + " sh -c " + shell_quoted(std::string("cat > ") + PAIRING_PREFS)), *pairing_);
}

bool UsbPhone::reverse(uint16_t port)  // [UsbPhone.md#reverse]
{
    const std::string spec = "tcp:" + std::to_string(port);
    if (run(adb("reverse " + spec + " " + spec)).status != 0)
        return false;
    undo_.push_back(adb("reverse --remove " + spec));
    return true;
}

bool UsbPhone::forward(uint16_t port)  // [UsbPhone.md#forward]
{
    const std::string spec = "tcp:" + std::to_string(port);
    if (run(adb("forward " + spec + " " + spec)).status != 0)
        return false;
    undo_.push_back(adb("forward --remove " + spec));
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

bool UsbPhone::tap(const std::string& string_name, std::chrono::seconds timeout)  // [UsbPhone.md#tap]
{
    const auto texts = translations_of(string_name);
    for (const auto end = std::chrono::steady_clock::now() + timeout; std::chrono::steady_clock::now() < end; std::this_thread::sleep_for(std::chrono::milliseconds(500))) {
        shell(std::string("uiautomator dump ") + UI_DUMP);
        const Output ui = run(adb("exec-out", std::string("cat ") + UI_DUMP));
        if (const auto centre = centre_of(ui.text, texts)) {
            shell("input tap " + std::to_string(centre->first) + " " + std::to_string(centre->second));
            return true;
        }
    }
    return false;
}

bool UsbPhone::write_file(const std::string& path, const std::string& content)
{
    const std::string dir = std::filesystem::path(path).parent_path().generic_string();
    return run_with_input(adb("exec-in", "mkdir -p " + shell_quoted(dir) + " && cat > " + shell_quoted(path)), content);
}

std::optional<std::string> UsbPhone::read_file(const std::string& path)
{
    int status = -1;
    shell("test -f " + shell_quoted(path), &status);
    if (status != 0)
        return std::nullopt;
    return run(adb("exec-out", "cat " + shell_quoted(path))).text;
}

void UsbPhone::remove_dir(const std::string& path)
{
    shell("rm -rf " + shell_quoted(path));
}

}
