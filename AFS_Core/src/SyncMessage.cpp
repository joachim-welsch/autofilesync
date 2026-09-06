#include "pch.h"
#include "SyncMessage.h"

namespace AFS_Core {

SyncMessage::SyncMessage(const std::wstring& msg, uint64_t filenumber, uint64_t filesize)
	: msg_(msg)
	, filenumber_(filenumber)
	, filesize_(filesize)
{}

} // namespace AFS_Core
