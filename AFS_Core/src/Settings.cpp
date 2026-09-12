#include "pch.h"
#include "Settings.h"

namespace AFS_Core {

void Settings::reset()  // [Settings.md#reset]
{
    const bool console_mode = console_mode_;
    *this = Settings();
    console_mode_ = console_mode;
}

} // namespace AFS_Core
