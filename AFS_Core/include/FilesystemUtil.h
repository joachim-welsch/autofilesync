#pragma once
#include <filesystem>
#include <string>
#include <optional>

namespace AFS_Core {

namespace FilesystemUtil
{
	enum class FileInfoResult { OK, FileNotExist, DriveNotMounted, CannotReadFileAttributes, InvalidFileTime, ErrorReadFileInfo };

	bool file_exists(const std::wstring& filename);
    FileInfoResult fileinfo(const std::wstring& fpathname, uint64_t& filesize, std::filesystem::file_time_type& last_write_time);
	std::wstring make_error_message(FileInfoResult result);
    bool touch(const std::wstring& fpathname);
    std::optional<std::filesystem::path> is_mountpoint(const std::filesystem::path& path);
    std::optional<std::filesystem::path> get_mount_root(const std::filesystem::path& in);
	std::string home_dir();
}
namespace fsu = FilesystemUtil;

} // namespace AFS_Core
