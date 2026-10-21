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

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace demo {

struct Options {  // [Options.md#Options]
    std::vector<std::string> scenes;  // [Options.md#scenes]
    std::filesystem::path root;  // [Options.md#root]
    std::filesystem::path target;
    std::filesystem::path telemetry_file;
    std::filesystem::path markers_file;
    std::filesystem::path trigger_file;  // [Options.md#start-trigger]
    bool wait_for_stdin = false;
    int start_delay_seconds = 3;
    int window_width = 1280;
    int window_height = 800;
    double speed = 1.0;  // [Options.md#speed]
    int cycle_seconds = 0;  // [Options.md#cycle_seconds]
    int assistant_wait_seconds = 90;
    std::wstring question = L"Was passiert, wenn ich die Festplatte abziehe?";
    std::string source_name = "Bilder";  // [Options.md#names]
    std::wstring sync_name = L"Fotos";
    bool keep_sandbox = false;
};

const std::vector<std::string>& all_scenes();
std::optional<Options> parse_options(int argc, char** argv);  // [Options.md#parse_options]

}
