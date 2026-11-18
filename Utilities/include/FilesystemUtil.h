/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include <UtilitiesConfig.h>
#include <system_error>
#include <filesystem>
#include <string>
#include <string_view>
#include <optional>
#include <Logging.h>

namespace Utilities {

namespace FilesystemUtil
{
	enum class FileInfoResult { OK, FileNotExist, DirectoryNotExist, DriveNotMounted, CannotReadFileAttributes, CannotSetFileAttributes,
        PermissionDenied, FileLocked, InvalidFileTime, ErrorReadFileInfo, ErrorWriteFileInfo, CannotCreateDirectory, DiskFull, LinksNotSupported, ServerUnreachable, ReadOnlyFilesystem };

	bool file_exists(const std::wstring& filename);
    FileInfoResult fileinfo(const std::wstring& fpathname, uint64_t& filesize, std::filesystem::file_time_type& last_write_time);
	std::wstring make_error_message(FileInfoResult result);
    bool touch(const std::wstring& fpathname);
    std::optional<std::filesystem::path> is_mountpoint(const std::filesystem::path& path);
    std::optional<std::filesystem::path> get_mount_root(const std::filesystem::path& in);
	std::string home_dir();
    std::optional<std::filesystem::path> env_path(const char* name);  // [FilesystemUtil.md#env_path]
    bool plain_entry_name(std::string_view name);  // [FilesystemUtil.md#plain_entry_name]
    bool volume_exists(const std::wstring& drive_name, const std::wstring& volume_name);
    bool drive_exists(const std::wstring& drive_name);
    void handle_last_error(std::filesystem::filesystem_error const& ex);
    struct ContentComparison {  // [FilesystemUtil.md#compare_content]
        bool equal = false;
        FileInfoResult refusal = FileInfoResult::OK;
        std::wstring refused_file;
    };
    FileInfoResult reason_for_failure(const std::error_code& ec);  // [FilesystemUtil.md#reason_for_failure]
    FileInfoResult reason_for_write_failure(const std::error_code& ec);  // [FilesystemUtil.md#reason_for_write_failure]
    std::error_code last_stream_error();  // [FilesystemUtil.md#last_stream_error]
    std::wstring failure_text(const std::error_code& ec);  // [FilesystemUtil.md#failure_text]
    FileInfoResult create_directory_refusal(const std::filesystem::path& directory, std::error_code& ec);  // [FilesystemUtil.md#create_directory_refusal]
    std::optional<uint64_t> free_space(const std::filesystem::path& directory);  // [FilesystemUtil.md#free_space]
#ifdef _TEST
    void set_free_space_for_test(std::optional<uint64_t> bytes);  // [FilesystemUtil.md#free_space]
#endif
    bool read_only_filesystem(const std::filesystem::path& path);  // [FilesystemUtil.md#read_only_filesystem]
#ifdef _TEST
    void set_read_only_filesystem_for_test(std::optional<std::filesystem::path> root);  // [FilesystemUtil.md#read_only_filesystem]
#endif
    FileInfoResult read_refusal(const std::wstring& file);  // [FilesystemUtil.md#read_refusal]
    ContentComparison compare_content(const std::wstring& input1, const std::wstring& input2);  // [FilesystemUtil.md#compare_content]
    bool files_equal(const std::wstring& input1, const std::wstring& input2);
    FileInfoResult rename_refusal(const std::wstring& oldname, const std::wstring& newname);  // [FilesystemUtil.md#rename_refusal]
    bool rename_file(const std::wstring& oldname, const std::wstring& newname);
    bool remove_file(const std::wstring& name, LogLevel ll = LogLevel::None);
    FileInfoResult remove_refusal(const std::wstring& name);  // [FilesystemUtil.md#remove_refusal]
    bool set_last_write_time(const std::wstring& fn, const std::filesystem::file_time_type& modify_time, LogLevel ll = LogLevel::None);
    FileInfoResult write_time_refusal(const std::wstring& file, const std::filesystem::file_time_type& time);  // [FilesystemUtil.md#write_time_refusal]
#ifdef _TEST
    void set_write_time_error_for_test(std::optional<std::error_code> error);  // [FilesystemUtil.md#write_time_refusal]
#endif
    FileInfoResult permissions_refusal(const std::wstring& file, std::filesystem::perms permissions);  // [FilesystemUtil.md#permissions_refusal]
#ifdef _TEST
    void set_permissions_error_for_test(std::optional<std::error_code> error);  // [FilesystemUtil.md#permissions_refusal]
#endif
    std::wstring read_utf8_file(const std::filesystem::path& file);  // [FilesystemUtil.md#utf8-files]
    void write_utf8_file(const std::filesystem::path& file, const std::wstring& text);  // [FilesystemUtil.md#utf8-files]
    bool write_private_file(const std::filesystem::path& file, std::string_view bytes);  // [FilesystemUtil.md#private-files]
}
namespace fsu = FilesystemUtil;

} // namespace Utilities
