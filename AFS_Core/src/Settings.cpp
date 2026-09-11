#include "pch.h"
#include "Settings.h"

namespace AFS_Core {

// Console mode comes from the command line, not from the configuration file,
// so it survives a reload of the configuration.
void Settings::reset()
{
    const bool console_mode = console_mode_;
    *this = Settings();
    console_mode_ = console_mode;
}

} // namespace AFS_Core
