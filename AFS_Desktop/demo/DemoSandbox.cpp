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
#include "DemoSandbox.h"
#include "SyncArbiter.h"
#include <wx/config.h>
#include <wx/display.h>
#include <wx/fileconf.h>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace demo {
namespace {

fs::path sandbox_root;
fs::path sandbox_source;
fs::path sandbox_target;

constexpr int PHOTOS_PER_YEAR = 8;
constexpr const char* YEARS[] = {"2023", "2024", "2025"};
constexpr std::size_t MIN_PHOTO_BYTES = 150 * 1024;  // [DemoSandbox.md#photo-sizes]
constexpr std::size_t MAX_PHOTO_BYTES = 450 * 1024;

void write_random_file(const fs::path& pathname, std::size_t bytes, std::mt19937& random)
{
    fs::create_directories(pathname.parent_path());
    std::ofstream file(pathname, std::ios::binary);
    std::string block(4096, '\0');
    for (std::size_t written = 0; written < bytes; written += block.size()) {
        for (char& byte : block)
            byte = static_cast<char>(random());
        file.write(block.data(), static_cast<std::streamsize>(std::min(block.size(), bytes - written)));
    }
}

void write_photos()
{
    std::mt19937 random(2026);  // [DemoSandbox.md#photo-sizes]
    for (const char* year : YEARS)
        for (int i = 1; i <= PHOTOS_PER_YEAR; ++i) {
            const std::size_t bytes = MIN_PHOTO_BYTES + random() % (MAX_PHOTO_BYTES - MIN_PHOTO_BYTES);
            const std::string name = std::string("IMG_") + year + "_" + (i < 10 ? "0" : "") + std::to_string(i) + ".jpg";
            write_random_file(DemoSandbox::source_dir() / year / name, bytes, random);
        }
    write_random_file(DemoSandbox::new_photo(), MAX_PHOTO_BYTES, random);
}

void write_file(const fs::path& pathname, const std::string& content)
{
    fs::create_directories(pathname.parent_path());
    std::ofstream(pathname) << content;
}

std::string demo_config()  // [DemoSandbox.md#demo_config]
{
    return std::string("{\n")
        + "  \"Settings\": {\"StartMinimzed\": \"false\", \"MinimizeInsteadOfClose\": \"true\","
          " \"Autostart\": \"false\", \"StartSyncAtStart\": \"false\","
          " \"ShowErrors\": \"true\", \"ShowWarnings\": \"false\", \"ShowInfos\": \"false\"},\n"
        + "  \"Logging\": {\"LogLevel\": \"Info\", \"LogFilePathName\": \""
        + (sandbox_root / "autofilesync.log").generic_string()
        + "\", \"RetentionDays\": \"1\", \"EditorPathName\": \"true\"}\n}\n";
}

void redirect_user_directories()  // [DemoSandbox.md#redirect_user_directories]
{
    const char* home = std::getenv("HOME");
    const char* data_home = std::getenv("XDG_DATA_HOME");
    const fs::path data = data_home != nullptr ? fs::path(data_home) : home != nullptr ? fs::path(home) / ".local" / "share" : fs::path();
    if (!data.empty())
        setenv("XDG_DATA_HOME", data.c_str(), 1);
    setenv("HOME", sandbox_root.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (sandbox_root / ".config").c_str(), 1);
#ifdef __linux__
    setenv("GTK_IM_MODULE", "gtk-im-context-simple", 1);
#endif
}

fs::path config_file()
{
#ifdef _MACOS
    return sandbox_root / "Library" / "Application Support" / "autofilesync" / "autofilesync.json";
#else
    return sandbox_root / ".config" / "autofilesync" / "autofilesync.json";
#endif
}

}

void DemoSandbox::create(const Options& options)
{
    sandbox_root = options.root;
    sandbox_source = options.root / options.source_name;
    sandbox_target = options.target;
    fs::remove_all(sandbox_root);
    fs::create_directories(sandbox_root);
    redirect_user_directories();
    AFS_Core::SyncArbiter::use_private_scope();
    write_file(config_file(), demo_config());
    write_photos();
    fs::create_directories(target_dir());
}

void DemoSandbox::isolate_preferences(const Options& options)  // [DemoSandbox.md#isolate_preferences]
{
    const wxRect area = wxDisplay(0u).GetClientArea();
    const int width = std::min(options.window_width, area.width);
    const int height = std::min(options.window_height, area.height);
    write_file(sandbox_root / "afs_demo.ini", "[MainWindow]\nx=" + std::to_string(area.x + (area.width - width) / 2)
        + "\ny=" + std::to_string(area.y + (area.height - height) / 2)
        + "\nwidth=" + std::to_string(width) + "\nheight=" + std::to_string(height) + "\n");
    wxConfigBase::Set(new wxFileConfig(wxEmptyString, wxEmptyString, (sandbox_root / "afs_demo.ini").string(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE));
}

void DemoSandbox::remove()
{
    fs::remove_all(sandbox_root);
    std::error_code ignored;
    fs::remove_all(sandbox_target, ignored);
}

const fs::path& DemoSandbox::root()
{
    return sandbox_root;
}

fs::path DemoSandbox::source_dir()
{
    return sandbox_source;
}

fs::path DemoSandbox::target_dir()
{
    return sandbox_target;
}

fs::path DemoSandbox::new_photo()
{
    return sandbox_root / "Kamera" / "IMG_2026_01.jpg";
}

fs::path DemoSandbox::photo_to_delete()
{
    return source_dir() / YEARS[0] / "IMG_2023_01.jpg";
}

}
