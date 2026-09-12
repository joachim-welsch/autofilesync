#pragma once

#include <filesystem>

namespace guitest {

class GuiTestSandbox {  // [GuiTestSandbox.md#GuiTestSandbox]
public:
    static void create();
    static void remove();
    static const std::filesystem::path& root();
    static std::filesystem::path configFile();
    static std::filesystem::path sourceDir();
    static std::filesystem::path targetDir();
};

}
