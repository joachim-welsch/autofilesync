	#pragma once
#include <string>
#include <vector>

namespace tu {
	bool erase_test_dir(const std::wstring& fn, bool display_error);
	void erase_ssh_test_dir(const std::wstring& ssh_path);  // [DirUtil.md#erase_ssh_test_dir]
	void erase_ssh_test_dir_variants(const std::wstring& ssh_base_path, const std::vector<std::wstring>& suffixes);  // [DirUtil.md#erase_ssh_test_dir_variants]
	bool check_copied_files(const std::wstring& source_dir, const std::wstring& dest_dir);
	std::string concat_path(const std::string& path1, const std::string& path2);
	std::wstring concat_path(const std::wstring& path1, const std::wstring& path2);
	std::string concat_temp(const std::string& path);
	std::wstring concat_temp(const std::wstring& path);
	std::string concat_ssh_temp(const std::string& path);
	std::wstring concat_ssh_temp(const std::wstring& path);
}
