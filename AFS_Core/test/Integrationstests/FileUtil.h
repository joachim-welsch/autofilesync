#pragma once
#include <string>
#include "SyncFileTests.h"

namespace tu {
	bool copyFile(const std::wstring& source, const std::wstring& destination);
	bool copySourceToDest(const std::wstring& source = SyncFileTests::SourcePathName, const std::wstring& destination = SyncFileTests::TargetPathName);
	bool eraseTestFile(const std::wstring& fn);
	std::wstring tempDirW();
	std::string tempDir();
	std::string testTempDir();
	std::string testSourceDir();
	std::string testDestDir();
	bool pathContains(std::wstring haystack, std::wstring needle);
}