#pragma once
#include <string>
#include <chrono>
#include <filesystem>
#include "SyncFileTests.h"

namespace tu {
	void write_text_file(const std::filesystem::path& file, const std::string& content,
		std::chrono::seconds age = std::chrono::seconds(0));  // [FileUtil.md#write_text_file]
	std::string read_text_file(const std::filesystem::path& file);
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