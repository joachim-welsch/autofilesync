#pragma once
#include <string>
#include "SyncFileTests.h"

namespace tu {
	bool copy_file(const std::wstring& source, const std::wstring& destination);
	bool copy_source_to_dest(const std::wstring& source = SyncFileTests::SOURCE_PATH_NAME, const std::wstring& destination = SyncFileTests::TARGET_PATH_NAME);
	bool erase_test_file(const std::wstring& fn);
	std::wstring temp_dir_w();
	std::string temp_dir();
	std::string test_temp_dir();
	std::string test_source_dir();
	std::string test_dest_dir();
	bool path_contains(std::wstring haystack, std::wstring needle);
}