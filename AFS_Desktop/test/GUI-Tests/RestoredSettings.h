/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
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
