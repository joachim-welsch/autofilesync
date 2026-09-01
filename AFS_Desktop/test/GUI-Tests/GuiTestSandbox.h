#pragma once

#include <filesystem>

namespace guitest {

// Throw-away HOME/XDG_CONFIG_HOME for the whole test run, so a GUI test can
// never read or damage the developer's real configuration. create() must run
// before wxWidgets starts: Config and wxStandardPaths resolve their paths from
// the environment as soon as the application initialises.
class GuiTestSandbox {
public:
    static void create();
    static void remove();
    static const std::filesystem::path& root();
    static std::filesystem::path configFile();
    static std::filesystem::path sourceDir();
    static std::filesystem::path targetDir();
};

}
