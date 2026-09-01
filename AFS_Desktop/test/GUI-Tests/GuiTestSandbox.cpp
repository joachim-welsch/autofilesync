#include "GuiTestSandbox.h"
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
// Both blocks are needed: the configuration reads its paths through the CRT
// environment (_wdupenv_s), wxWidgets through the Win32 one.
void setEnvironmentVariable(const wchar_t* name, const fs::path& value)
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
    // Config::getRootPathname() builds the configuration path from APPDATA and
    // the path it migrates from out of USERPROFILE; wxGetHomeDir() asks HOME first.
    setEnvironmentVariable(L"APPDATA", GuiTestSandbox::root() / "AppData");
    setEnvironmentVariable(L"USERPROFILE", GuiTestSandbox::root());
    setEnvironmentVariable(L"HOME", GuiTestSandbox::root());
#else
    setEnvironmentVariable("HOME", GuiTestSandbox::root());
    setEnvironmentVariable("XDG_CONFIG_HOME", GuiTestSandbox::root() / ".config");
#endif
}

void writeFile(const fs::path& pathname, const std::string& content)
{
    fs::create_directories(pathname.parent_path());
    std::ofstream(pathname) << content;
}

// Enough of a configuration to keep the start-up quiet: an unreadable or
// missing file makes Config::read() pop up a message box before any test runs.
// The log path is written with forward slashes - a Windows path with
// backslashes would be an invalid escape sequence for the JSON parser.
std::string minimalConfig()
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
    writeFile(configFile(), minimalConfig());
    writeFile(sourceDir() / "beispiel.txt", "Inhalt der Quelldatei\n");
    fs::create_directories(targetDir());
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
