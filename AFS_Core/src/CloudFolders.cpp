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
#include "pch.h"
#include "CloudFolders.h"
#include "FilesystemUtil.h"
#include "StrUtil.h"
#include <algorithm>
#include <fstream>
#include <string_view>
#include <tuple>
#include <system_error>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#ifdef _WIN32
    #include <Windows.h>
#endif

namespace AFS_Core {

namespace cloud_folders {

namespace fs = std::filesystem;
namespace pt = boost::property_tree;

namespace {
constexpr wchar_t DROPBOX[] = L"Dropbox";
constexpr wchar_t DROPBOX_BUSINESS[] = L"Dropbox Business";
constexpr wchar_t NEXTCLOUD[] = L"Nextcloud";
constexpr std::string_view NEXTCLOUD_FOLDER_GROUP = "\\Folders";
constexpr std::string_view NEXTCLOUD_PATH_KEY = "\\localPath";

fs::path utf8_path(std::string_view text)
{
    return fs::path(std::u8string(text.begin(), text.end()));
}

void add_dropbox(CloudFolders& found, const fs::path& info_file)  // [CloudFolders.md#dropbox]
{
    std::ifstream in(info_file);
    pt::ptree accounts;
    try {
        pt::read_json(in, accounts);
    }
    catch (const pt::json_parser_error&) {
        return;
    }
    if (const auto personal = accounts.get_optional<std::string>("personal.path"))
        found.push_back({DROPBOX, utf8_path(*personal)});
    if (const auto business = accounts.get_optional<std::string>("business.path"))
        found.push_back({DROPBOX_BUSINESS, utf8_path(*business)});
}

bool is_nextcloud_folder_key(std::string_view key)
{
    return key.ends_with(NEXTCLOUD_PATH_KEY) && key.find(NEXTCLOUD_FOLDER_GROUP) != std::string_view::npos;
}

std::string_view unquoted(std::string_view value)
{
    while (!value.empty() && (value.back() == '\r' || value.back() == ' '))
        value.remove_suffix(1);
    const bool quoted = value.size() >= 2 && value.front() == '"' && value.back() == '"';
    return quoted ? value.substr(1, value.size() - 2) : value;
}

void add_nextcloud(CloudFolders& found, const fs::path& config_file)  // [CloudFolders.md#nextcloud]
{
    std::ifstream in(config_file);
    for (std::string line; std::getline(in, line);) {
        const auto separator = line.find('=');
        if (separator != std::string::npos && is_nextcloud_folder_key(std::string_view(line).substr(0, separator)))
            found.push_back({NEXTCLOUD, utf8_path(unquoted(std::string_view(line).substr(separator + 1)))});
    }
}

#ifdef _WIN32
constexpr wchar_t ONE_DRIVE[] = L"OneDrive";
constexpr wchar_t GOOGLE_DRIVE[] = L"Google Drive";

void add_one_drive(CloudFolders& found)  // [CloudFolders.md#onedrive]
{
    for (const char* variable : {"OneDriveConsumer", "OneDriveCommercial", "OneDrive"})
        if (const auto folder = fsu::env_path(variable))
            found.push_back({ONE_DRIVE, *folder});
}

bool is_google_drive(const std::wstring& drive_root)  // [CloudFolders.md#google-drive-windows]
{
    wchar_t label[MAX_PATH + 1] = {};
    return GetDriveTypeW(drive_root.c_str()) == DRIVE_FIXED
        && GetVolumeInformationW(drive_root.c_str(), label, MAX_PATH + 1, nullptr, nullptr, nullptr, nullptr, 0)
        && std::wstring_view(label) == GOOGLE_DRIVE;
}

void add_google_drive(CloudFolders& found)
{
    DWORD drives = GetLogicalDrives();
    for (wchar_t letter = L'A'; drives != 0; drives >>= 1, ++letter) {
        const std::wstring drive_root = std::wstring(1, letter) + L":\\";
        if ((drives & 1) && is_google_drive(drive_root))
            found.push_back({GOOGLE_DRIVE, drive_root});
    }
}

CloudFolders candidates()
{
    CloudFolders found;
    add_one_drive(found);
    add_google_drive(found);
    for (const char* variable : {"APPDATA", "LOCALAPPDATA"})
        if (const auto data_dir = fsu::env_path(variable))
            add_dropbox(found, *data_dir / "Dropbox" / "info.json");
    if (const auto roaming = fsu::env_path("APPDATA"))
        add_nextcloud(found, *roaming / "Nextcloud" / "nextcloud.cfg");
    return found;
}
#elif defined(_MACOS)
constexpr wchar_t ICLOUD_DRIVE[] = L"iCloud Drive";

std::wstring provider_of(const std::string& storage_name)  // [CloudFolders.md#cloud-storage-macos]
{
    const std::string product = storage_name.substr(0, storage_name.find('-'));
    return product == "GoogleDrive" ? L"Google Drive" : util::string_to_wstring(product);
}

void add_cloud_storage(CloudFolders& found, const fs::path& storage_dir)  // [CloudFolders.md#cloud-storage-macos]
{
    std::error_code ec;
    for (fs::directory_iterator it(storage_dir, ec), end; !ec && it != end; it.increment(ec))
        found.push_back({provider_of(it->path().filename().string()), it->path()});
}

CloudFolders candidates()
{
    const fs::path home = fsu::home_dir();
    if (home.empty())
        return {};
    CloudFolders found;
    add_cloud_storage(found, home / "Library" / "CloudStorage");
    found.push_back({ICLOUD_DRIVE, home / "Library" / "Mobile Documents" / "com~apple~CloudDocs"});
    add_dropbox(found, home / ".dropbox" / "info.json");
    add_nextcloud(found, home / "Library" / "Preferences" / "Nextcloud" / "nextcloud.cfg");
    return found;
}
#else
CloudFolders candidates()
{
    const fs::path home = fsu::home_dir();
    if (home.empty())
        return {};
    CloudFolders found;
    add_dropbox(found, home / ".dropbox" / "info.json");
    add_nextcloud(found, fsu::env_path("XDG_CONFIG_HOME").value_or(home / ".config") / "Nextcloud" / "nextcloud.cfg");
    return found;
}
#endif

bool resolve(CloudFolder& folder)  // [CloudFolders.md#resolved-paths]
{
    std::error_code ec;
    const fs::path resolved = fs::canonical(folder.path, ec);
    if (ec || !fs::is_directory(resolved, ec))
        return false;
    folder.path = resolved;
    return true;
}

bool known(const CloudFolders& folders, const fs::path& path)
{
    return std::ranges::any_of(folders, [&](const CloudFolder& folder) { return folder.path == path; });
}
}

CloudFolders detect()
{
    CloudFolders folders;
    for (CloudFolder& candidate : candidates())
        if (resolve(candidate) && !known(folders, candidate.path))
            folders.push_back(std::move(candidate));
    std::ranges::sort(folders, [](const CloudFolder& a, const CloudFolder& b) { return std::tie(a.provider, a.path) < std::tie(b.provider, b.path); });
    return folders;
}

}

} // namespace AFS_Core
