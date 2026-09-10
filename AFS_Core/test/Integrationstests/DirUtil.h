	#pragma once
#include <string>
#include <vector>

namespace tu {
	bool erase_test_dir(const std::wstring& fn, bool display_error);
	// Removes files under an "account:/remote/path" tree via SFTP. Needed
	// wherever a test writes through the SSH test account: that account can
	// own files a plain fs::remove_all() (running as the developer's own
	// user) has no permission to touch, which would otherwise leave a
	// previous run's files in place and make the next run see stale state.
	void erase_ssh_test_dir(const std::wstring& ssh_path);
	// Same reasoning as erase_ssh_test_dir(), but for fixtures whose tests each
	// derive their own destination by appending a suffix (e.g. "..._many",
	// "..._mixed") to a shared base name: clears every named variant in one
	// call. Takes the explicit suffix list rather than listing the parent
	// directory over SFTP - that parent is the shared system temp dir, and
	// an unrelated entry there with an unconvertible name would make the
	// listing itself throw.
	void erase_ssh_test_dir_variants(const std::wstring& ssh_base_path, const std::vector<std::wstring>& suffixes);
	bool check_copied_files(const std::wstring& source_dir, const std::wstring& dest_dir);
	std::string concat_path(const std::string& path1, const std::string& path2);
	std::wstring concat_path(const std::wstring& path1, const std::wstring& path2);
	std::string concat_temp(const std::string& path);
	std::wstring concat_temp(const std::wstring& path);
	std::string concat_ssh_temp(const std::string& path);
	std::wstring concat_ssh_temp(const std::wstring& path);
}
