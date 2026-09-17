#include "pch.h"
#include "SyncMessage.h"

namespace AFS_Core {

SyncMessage::SyncMessage(const std::wstring& msg, const std::wstring& file, uint64_t filenumber, uint64_t filesize)
	: msg_(msg)
	, file_(file)
	, filenumber_(filenumber)
	, filesize_(filesize)
{}

} // namespace AFS_Core
