	#pragma once
#include <string>
#include <vector>

namespace tu {
	bool eraseTestDir(const std::wstring& fn, bool displayError);
	// Removes files under an "account:/remote/path" tree via SFTP. Needed
	// wherever a test writes through the SSH test account: that account can
	// own files a plain fs::remove_all() (running as the developer's own
	// user) has no permission to touch, which would otherwise leave a
	// previous run's files in place and make the next run see stale state.
	void eraseSshTestDir(const std::wstring& sshPath);
	// Same reasoning as eraseSshTestDir(), but for fixtures whose tests each
	// derive their own destination by appending a suffix (e.g. "..._many",
	// "..._mixed") to a shared base name: clears every named variant in one
	// call. Takes the explicit suffix list rather than listing the parent
	// directory over SFTP - that parent is the shared system temp dir, and
	// an unrelated entry there with an unconvertible name would make the
	// listing itself throw.
	void eraseSshTestDirVariants(const std::wstring& sshBasePath, const std::vector<std::wstring>& suffixes);
	bool checkCopiedFiles(const std::wstring& sourceDir, const std::wstring& destDir);
	std::string concatPath(const std::string& path1, const std::string& path2);
	std::wstring concatPath(const std::wstring& path1, const std::wstring& path2);
	std::string concatTemp(const std::string& path);
	std::wstring concatTemp(const std::wstring& path);
	std::string concatSSHTemp(const std::string& path);
	std::wstring concatSSHTemp(const std::wstring& path);
}
