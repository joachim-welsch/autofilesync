#pragma once

#include <string>
#include "TestPaths.h"

namespace SyncDirTests
{
#ifdef _UNIX
    const std::wstring SOURCE_PATH = TestPaths::repo_dir() + L"/test/resources/dir/";
    const std::wstring TARGET_PATH = TestPaths::scratch_dir() + L"/dir";
#else
    const std::wstring SOURCE_PATH = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\resources\\dir\\";
    const std::wstring TARGET_PATH = L"C:\\temp\\test\\dir";
#endif
}
