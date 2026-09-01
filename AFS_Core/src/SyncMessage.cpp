#include "pch.h"
#include "SyncMessage.h"

SyncMessage::SyncMessage(const std::wstring& msg, uint64_t filenumber, uint64_t filesize)
	: msg_(msg)
	, filenumber_(filenumber)
	, filesize_(filesize)
{}
