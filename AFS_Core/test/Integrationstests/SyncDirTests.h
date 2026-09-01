#pragma once

#include <string>
#include "TestPaths.h"

namespace SyncDirTests
{
#ifdef _UNIX
    const std::wstring SourcePath = TestPaths::repoDir() + L"/test/resources/dir/";
    const std::wstring TargetPath = TestPaths::scratchDir() + L"/dir";
#else
	const std::wstring SourcePath = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\resources\\dir\\";
	const std::wstring TargetPath = L"C:\\temp\\test\\dir";
#endif
}
