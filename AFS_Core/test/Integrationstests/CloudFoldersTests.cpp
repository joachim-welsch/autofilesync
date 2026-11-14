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
#include <filesystem>
#include <fstream>
#include <list>
#include <string>
#include "ScopedEnv.h"
#include "TestPaths.h"

using namespace AFS_Core;
namespace fs = std::filesystem;

namespace {

class CloudFoldersTest : public ::testing::Test {  // [CloudFoldersTests.md#CloudFoldersTest]
protected:
    fs::path test_home_;
    std::list<ScopedEnv> redirected_;

    fs::path dropbox_info_file() const
    {
#ifdef _WIN32
        return test_home_ / "Roaming" / "Dropbox" / "info.json";
#else
        return test_home_ / ".dropbox" / "info.json";
#endif
    }

    fs::path nextcloud_config_file() const
    {
#ifdef _WIN32
        return test_home_ / "Roaming" / "Nextcloud" / "nextcloud.cfg";
#elif defined(_MACOS)
        return test_home_ / "Library" / "Preferences" / "Nextcloud" / "nextcloud.cfg";
#else
        return test_home_ / ".config" / "Nextcloud" / "nextcloud.cfg";
#endif
    }

    fs::path make_folder(const std::string& name) const
    {
        const fs::path folder = test_home_ / fs::path(std::u8string(name.begin(), name.end()));
        fs::create_directories(folder);
        return fs::canonical(folder);
    }

    static void write(const fs::path& file, const std::string& content)
    {
        fs::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << content;
    }

    static CloudFolders folders_of(const std::wstring& provider)  // [CloudFoldersTests.md#folders_of]
    {
        CloudFolders matching;
        for (const CloudFolder& folder : cloud_folders::detect())
            if (folder.provider.starts_with(provider))
                matching.push_back(folder);
        return matching;
    }

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        test_home_ = fs::temp_directory_path() / ("autofilesync_cloudtest_" + test_name + "_" + std::to_string(TestPaths::process_id()));
        fs::remove_all(test_home_);
        fs::create_directories(test_home_);
        test_home_ = fs::canonical(test_home_);
#ifdef _WIN32
        redirected_.emplace_back("APPDATA", (test_home_ / "Roaming").string().c_str());
        redirected_.emplace_back("LOCALAPPDATA", (test_home_ / "Local").string().c_str());
        for (const char* variable : {"OneDrive", "OneDriveConsumer", "OneDriveCommercial"})
            redirected_.emplace_back(variable, nullptr);
#else
        redirected_.emplace_back("HOME", test_home_.string().c_str());
        redirected_.emplace_back("XDG_CONFIG_HOME", nullptr);
#endif
    }

    void TearDown() override
    {
        redirected_.clear();
        fs::remove_all(test_home_);
    }
};

TEST_F(CloudFoldersTest, NothingInstalledFindsNothing)
{
    EXPECT_TRUE(folders_of(L"Dropbox").empty());
    EXPECT_TRUE(folders_of(L"Nextcloud").empty());
#ifndef _WIN32
    EXPECT_TRUE(cloud_folders::detect().empty());
#endif
}

TEST_F(CloudFoldersTest, DropboxPersonalAndBusinessFolders)
{
    const fs::path personal = make_folder("Dropbox");
    const fs::path business = make_folder("Dropbox (Firma)");
    write(dropbox_info_file(), "{\"personal\": {\"path\": \"" + personal.generic_string() + "\", \"host\": 1},"
                               " \"business\": {\"path\": \"" + business.generic_string() + "\", \"host\": 2}}");
    EXPECT_EQ(folders_of(L"Dropbox"), (CloudFolders{{L"Dropbox", personal}, {L"Dropbox Business", business}}));
}

TEST_F(CloudFoldersTest, DropboxFolderWithEscapedUmlaut)
{
    const fs::path folder = make_folder("Dropbox J\xC3\xBCrgen");
    write(dropbox_info_file(), "{\"personal\": {\"path\": \"" + test_home_.generic_string() + "/Dropbox J\\u00fcrgen\"}}");
    EXPECT_EQ(folders_of(L"Dropbox"), (CloudFolders{{L"Dropbox", folder}}));
}

TEST_F(CloudFoldersTest, BrokenDropboxFileFindsNothing)
{
    make_folder("Dropbox");
    write(dropbox_info_file(), "{\"personal\": {\"path\": ");
    EXPECT_TRUE(folders_of(L"Dropbox").empty());
}

TEST_F(CloudFoldersTest, FolderThatNoLongerExistsIsLeftOut)
{
    write(dropbox_info_file(), "{\"personal\": {\"path\": \"" + (test_home_ / "Dropbox").generic_string() + "\"}}");
    EXPECT_TRUE(folders_of(L"Dropbox").empty());
}

TEST_F(CloudFoldersTest, FileInPlaceOfTheFolderIsLeftOut)
{
    write(test_home_ / "Dropbox", "not a directory");
    write(dropbox_info_file(), "{\"personal\": {\"path\": \"" + (test_home_ / "Dropbox").generic_string() + "\"}}");
    EXPECT_TRUE(folders_of(L"Dropbox").empty());
}

TEST_F(CloudFoldersTest, NextcloudFoldersOfAllGroups)  // [CloudFoldersTests.md#NextcloudFoldersOfAllGroups]
{
    const fs::path documents = make_folder("Nextcloud");
    const fs::path photos = make_folder("Nextcloud Fotos");
    write(nextcloud_config_file(),
        "[General]\r\nclientVersion=3.13.0\r\n\r\n[Accounts]\r\nversion=2\r\n0\\url=https://cloud.example.org\r\n"
        "0\\Folders\\1\\localPath=" + documents.generic_string() + "/\r\n"
        "0\\Folders\\1\\targetPath=/\r\n"
        "0\\FoldersWithPlaceholders\\2\\localPath=\"" + photos.generic_string() + "/\"\r\n");
    EXPECT_EQ(folders_of(L"Nextcloud"), (CloudFolders{{L"Nextcloud", documents}, {L"Nextcloud", photos}}));
}

TEST_F(CloudFoldersTest, SameFolderIsListedOnce)
{
    const fs::path folder = make_folder("Nextcloud");
    write(nextcloud_config_file(),
        "[Accounts]\n0\\Folders\\1\\localPath=" + folder.generic_string() + "/\n"
        "0\\Folders\\2\\localPath=" + (folder / ".." / "Nextcloud").generic_string() + "\n");
    EXPECT_EQ(folders_of(L"Nextcloud"), (CloudFolders{{L"Nextcloud", folder}}));
}

#if !defined(_WIN32)
TEST_F(CloudFoldersTest, FolderBehindSymlinkIsListedUnderItsRealPath)
{
    const fs::path folder = make_folder("Speicher/Dropbox");
    fs::create_directory_symlink(folder, test_home_ / "Dropbox");
    write(dropbox_info_file(), "{\"personal\": {\"path\": \"" + (test_home_ / "Dropbox").generic_string() + "\"}}");
    EXPECT_EQ(folders_of(L"Dropbox"), (CloudFolders{{L"Dropbox", folder}}));
}
#endif

#if !defined(_WIN32) && !defined(_MACOS)
TEST_F(CloudFoldersTest, NextcloudConfigFollowsXdgConfigHome)
{
    const fs::path folder = make_folder("Nextcloud");
    const fs::path config_home = test_home_ / "einstellungen";
    ScopedEnv xdg("XDG_CONFIG_HOME", config_home.string().c_str());
    write(config_home / "Nextcloud" / "nextcloud.cfg", "[Accounts]\n0\\Folders\\1\\localPath=" + folder.generic_string() + "/\n");
    EXPECT_EQ(folders_of(L"Nextcloud"), (CloudFolders{{L"Nextcloud", folder}}));
}
#endif

#ifdef _MACOS
TEST_F(CloudFoldersTest, CloudStorageFoldersAreNamedAfterTheirProvider)
{
    const fs::path one_drive = make_folder("Library/CloudStorage/OneDrive-Personal");
    const fs::path google_drive = make_folder("Library/CloudStorage/GoogleDrive-name@example.com");
    const fs::path icloud = make_folder("Library/Mobile Documents/com~apple~CloudDocs");
    EXPECT_EQ(cloud_folders::detect(), (CloudFolders{{L"Google Drive", google_drive}, {L"OneDrive", one_drive}, {L"iCloud Drive", icloud}}));
}
#endif

}
