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

fs::path sandboxRoot;

#ifdef _WIN32
void setEnvironmentVariable(const wchar_t* name, const fs::path& value)  // [GuiTestSandbox.md#setEnvironmentVariable]
{
    _wputenv_s(name, value.c_str());
    SetEnvironmentVariableW(name, value.c_str());
}
#else
void setEnvironmentVariable(const char* name, const fs::path& value)
{
    setenv(name, value.c_str(), 1);
}
#endif

int currentProcessId()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

void redirectUserDirectories()
{
#ifdef _WIN32
    setEnvironmentVariable(L"APPDATA", GuiTestSandbox::root() / "AppData");  // [GuiTestSandbox.md#which-variables-matter]
    setEnvironmentVariable(L"USERPROFILE", GuiTestSandbox::root());
    setEnvironmentVariable(L"HOME", GuiTestSandbox::root());
    setEnvironmentVariable(L"HOMEDRIVE", GuiTestSandbox::root().root_name());  // [GuiTestSandbox.md#homedrive-homepath]
    setEnvironmentVariable(L"HOMEPATH", GuiTestSandbox::root().relative_path().wstring().insert(0, L"\\"));
#else
    setEnvironmentVariable("HOME", GuiTestSandbox::root());
    setEnvironmentVariable("XDG_CONFIG_HOME", GuiTestSandbox::root() / ".config");
#endif
}

void isolateInputMethod()  // [GuiTestSandbox.md#isolateInputMethod]
{
#ifdef __linux__
    setenv("GTK_IM_MODULE", "gtk-im-context-simple", 1);
#endif
}

void writeFile(const fs::path& pathname, const std::string& content)
{
    fs::create_directories(pathname.parent_path());
    std::ofstream(pathname) << content;
}

std::string minimalConfig()  // [GuiTestSandbox.md#minimalConfig]
{
    return std::string("{\n")
        + "  \"Settings\": {\"StartMinimzed\": \"false\", \"MinimizeInsteadOfClose\": \"false\","
          " \"Autostart\": \"false\", \"StartSyncAtStart\": \"false\","
          " \"ShowErrors\": \"false\", \"ShowWarnings\": \"false\", \"ShowInfos\": \"false\"},\n"
        + "  \"Logging\": {\"LogLevel\": \"Error\", \"LogFilePathName\": \""
        + (sandboxRoot / "autofilesync.log").generic_string()
        + "\", \"RetentionDays\": \"1\", \"EditorPathName\": \"true\"}\n}\n";
}

}

void GuiTestSandbox::create()
{
    sandboxRoot = fs::temp_directory_path() / ("autofilesync-guitest-" + std::to_string(currentProcessId()));
    fs::remove_all(sandboxRoot);
    fs::create_directories(sandboxRoot);
    redirectUserDirectories();
    isolateInputMethod();
    writeFile(configFile(), minimalConfig());
    writeFile(sourceDir() / "beispiel.txt", "Inhalt der Quelldatei\n");
    fs::create_directories(targetDir());
}

void GuiTestSandbox::isolatePreferences()  // [GuiTestSandbox.md#isolatePreferences]
{
    wxConfigBase::Set(new wxFileConfig(wxEmptyString, wxEmptyString, (sandboxRoot / "GuiTests.ini").string(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE));
}

void GuiTestSandbox::remove()
{
    fs::remove_all(sandboxRoot);
}

const fs::path& GuiTestSandbox::root()
{
    return sandboxRoot;
}

fs::path GuiTestSandbox::configFile()
{
#ifdef _WIN32
    return sandboxRoot / "AppData" / "autofilesync" / "autofilesync.json";
#elif defined(_MACOS)
    return sandboxRoot / "Library" / "Application Support" / "autofilesync" / "autofilesync.json";  // [GuiTestSandbox.md#configFile]
#else
    return sandboxRoot / ".config" / "autofilesync" / "autofilesync.json";
#endif
}

fs::path GuiTestSandbox::sourceDir()
{
    return sandboxRoot / "quelle";
}

fs::path GuiTestSandbox::targetDir()
{
    return sandboxRoot / "ziel";
}

}
