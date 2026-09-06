#pragma once
#include <string>
#include <vector>
#include "Global.h"

namespace AFS_Core {

class Directory {
	std::wstring current_ = L"\\";
public:
	WStringVector read(const std::wstring& path);
};

} // namespace AFS_Core
