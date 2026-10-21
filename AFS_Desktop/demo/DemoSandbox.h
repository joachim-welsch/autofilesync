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
#pragma once

#include "Options.h"
#include <filesystem>

namespace demo {

class DemoSandbox {  // [DemoSandbox.md#DemoSandbox]
public:
    static void create(const Options& options);
    static void isolate_preferences(const Options& options);  // [DemoSandbox.md#isolate_preferences]
    static void remove();
    static const std::filesystem::path& root();
    static std::filesystem::path source_dir();
    static std::filesystem::path target_dir();
    static std::filesystem::path new_photo();  // [DemoSandbox.md#new_photo]
    static std::filesystem::path photo_to_delete();
};

}
