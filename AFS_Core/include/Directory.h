#pragma once
#include <string>
#include <vector>

using svect = std::vector<std::wstring>;

class Directory {
	std::wstring current_ = L"\\";
public:
	svect read(const std::wstring& path);
};

