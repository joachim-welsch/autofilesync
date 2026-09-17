#pragma once

#include <string>
#include <cstdint>

namespace AFS_Core {

class SyncMessage {
	std::wstring	msg_;
	std::wstring	file_;
	uint64_t		filenumber_ = 0;
	uint64_t		filesize_ = 0;
public:
	SyncMessage() = default;
	SyncMessage(const std::wstring& msg, const std::wstring& file, uint64_t filenumber, uint64_t filesize);
	std::wstring msg() const { return msg_; }
	std::wstring file() const { return file_; }  // empty unless a file is being copied
	uint64_t    filenumber() const { return filenumber_; }
	uint64_t	filesize() const { return filesize_; }
};

} // namespace AFS_Core
