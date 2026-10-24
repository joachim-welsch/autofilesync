/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#pragma once
#include "ScopedEnv.h"
#include <filesystem>

namespace tu {
class ScopedConfigHome {  // [ScopedConfigHome.md#ScopedConfigHome]
#ifdef _WIN32
    static constexpr const char* VARIABLE = "APPDATA";
#elif defined(_MACOS)
    static constexpr const char* VARIABLE = "HOME";
#else
    static constexpr const char* VARIABLE = "XDG_CONFIG_HOME";
#endif
    const std::filesystem::path home_;
    ScopedEnv env_;
public:
    explicit ScopedConfigHome(std::filesystem::path home): home_(std::move(home)), env_(VARIABLE, home_.string().c_str()) {}
    std::filesystem::path config_dir() const
    {
#ifdef _MACOS
        return home_ / "Library" / "Application Support" / "autofilesync";
#else
        return home_ / "autofilesync";
#endif
    }
};
} // namespace tu
