#pragma once

#include "Config.h"
#include "RecycleBin.h"
#include "Settings.h"
#include <filesystem>

namespace guitest {

class RestoredSettings {  // [RestoredSettings.md#RestoredSettings]
public:
    RestoredSettings(): settings_(AFS_Core::Config::instance().settings()), binPath_(AFS_Core::RecycleBin::instance().bin_path()) {}
    ~RestoredSettings()
    {
        AFS_Core::Config::instance().settings() = settings_;
        AFS_Core::RecycleBin::instance().bin_path(binPath_);
    }
    RestoredSettings(const RestoredSettings&) = delete;
    RestoredSettings& operator=(const RestoredSettings&) = delete;
private:
    AFS_Core::Settings settings_;
    std::filesystem::path binPath_;
};

}
