#pragma once
#include <filesystem>
#include <string>
#include <optional>

namespace AFS_Core {

namespace FilesystemUtil
{
	enum class FileInfoResult { OK, FileNotExist, DriveNotMounted, CannotReadFileAttributes, InvalidFileTime, ErrorReadFileInfo };

	bool fileExists(const std::wstring& StrFilename);
    FileInfoResult fileinfo(const std::wstring& fpathname, uint64_t& filesize, std::filesystem::file_time_type& last_write_time);
	std::wstring makeErrorMessage(FileInfoResult result);
    bool touch(const std::wstring& fpathname);
    std::optional<std::filesystem::path> isMountpoint(const std::filesystem::path& path);
    std::optional<std::filesystem::path> getMountRoot(const std::filesystem::path& in);
	std::string homeDir();
}
namespace fsu = FilesystemUtil;

} // namespace AFS_Core
