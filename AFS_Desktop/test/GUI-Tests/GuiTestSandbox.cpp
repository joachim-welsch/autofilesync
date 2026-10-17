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
#include "GuiTestSandbox.h"
#include <wx/config.h>
#include <wx/fileconf.h>
#include <cstdlib>
#include <fstream>
#include <string>
#ifdef _WIN32
    #include <process.h>
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace guitest {
namespace {

fs::path sandbox_root;

#ifdef _WIN32
void set_environment_variable(const wchar_t* name, const fs::path& value)  // [GuiTestSandbox.md#set_environment_variable]
{
    _wputenv_s(name, value.c_str());
    SetEnvironmentVariableW(name, value.c_str());
}
#else
void set_environment_variable(const char* name, const fs::path& value)
{
    setenv(name, value.c_str(), 1);
}
#endif

int current_process_id()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

void redirect_user_directories()
{
#ifdef _WIN32
    set_environment_variable(L"APPDATA", GuiTestSandbox::root() / "AppData");  // [GuiTestSandbox.md#which-variables-matter]
    set_environment_variable(L"USERPROFILE", GuiTestSandbox::root());
    set_environment_variable(L"HOME", GuiTestSandbox::root());
    set_environment_variable(L"HOMEDRIVE", GuiTestSandbox::root().root_name());  // [GuiTestSandbox.md#homedrive-homepath]
    set_environment_variable(L"HOMEPATH", GuiTestSandbox::root().relative_path().wstring().insert(0, L"\\"));
#else
    set_environment_variable("HOME", GuiTestSandbox::root());
    set_environment_variable("XDG_CONFIG_HOME", GuiTestSandbox::root() / ".config");
#endif
}

void isolate_input_method()  // [GuiTestSandbox.md#isolate_input_method]
{
#ifdef __linux__
    setenv("GTK_IM_MODULE", "gtk-im-context-simple", 1);
#endif
}

void write_file(const fs::path& pathname, const std::string& content)
{
    fs::create_directories(pathname.parent_path());
    std::ofstream(pathname) << content;
}

std::string minimal_config()  // [GuiTestSandbox.md#minimal_config]
{
    return std::string("{\n")
        + "  \"Settings\": {\"StartMinimzed\": \"false\", \"MinimizeInsteadOfClose\": \"false\","
          " \"Autostart\": \"false\", \"StartSyncAtStart\": \"false\","
          " \"ShowErrors\": \"false\", \"ShowWarnings\": \"false\", \"ShowInfos\": \"false\"},\n"
        + "  \"Logging\": {\"LogLevel\": \"Error\", \"LogFilePathName\": \""
        + (sandbox_root / "autofilesync.log").generic_string()
        + "\", \"RetentionDays\": \"1\", \"EditorPathName\": \"true\"}\n}\n";
}

}

void GuiTestSandbox::create()
{
    sandbox_root = fs::temp_directory_path() / ("autofilesync-guitest-" + std::to_string(current_process_id()));
    fs::remove_all(sandbox_root);
    fs::create_directories(sandbox_root);
    redirect_user_directories();
    isolate_input_method();
    write_file(config_file(), minimal_config());
    write_file(source_dir() / "beispiel.txt", "Inhalt der Quelldatei\n");
    fs::create_directories(target_dir());
}

void GuiTestSandbox::isolate_preferences()  // [GuiTestSandbox.md#isolate_preferences]
{
    wxConfigBase::Set(new wxFileConfig(wxEmptyString, wxEmptyString, (sandbox_root / "GuiTests.ini").string(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE));
}

void GuiTestSandbox::remove()
{
    fs::remove_all(sandbox_root);
}

const fs::path& GuiTestSandbox::root()
{
    return sandbox_root;
}

fs::path GuiTestSandbox::config_file()
{
#ifdef _WIN32
    return sandbox_root / "AppData" / "autofilesync" / "autofilesync.json";
#elif defined(_MACOS)
    return sandbox_root / "Library" / "Application Support" / "autofilesync" / "autofilesync.json";  // [GuiTestSandbox.md#config_file]
#else
    return sandbox_root / ".config" / "autofilesync" / "autofilesync.json";
#endif
}

fs::path GuiTestSandbox::source_dir()
{
    return sandbox_root / "quelle";
}

fs::path GuiTestSandbox::target_dir()
{
    return sandbox_root / "ziel";
}

}
