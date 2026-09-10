#pragma once

#include <string>
#include "TestPaths.h"

namespace SyncTreeTests
{
#ifdef _UNIX
	const std::wstring SOURCE_PATH = TestPaths::repo_dir() + L"/test/resources/tree/";
	const std::wstring TARGET_PATH = TestPaths::scratch_dir() + L"/tree";
	const std::wstring RESOURCE_PATH = TestPaths::repo_dir() + L"/test/tree/source/";
	const std::wstring COUNT_TEST_SOURCE_PATH = TestPaths::scratch_dir() + L"/tree/src/";
	const std::wstring COUNT_TEST_TARGET_PATH = TestPaths::scratch_dir() + L"/tree/dest";
#else
	const std::wstring SourcePath = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\resources\\tree\\";
	const std::wstring TargetPath = L"C:\\temp\\test\\tree";
	const std::wstring ResourcePath = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\tree\\source\\";
	const std::wstring CountTestSourcePath = L"C:\\temp\\test\\tree\\src\\";
	const std::wstring CountTestTargetPath = L"C:\\temp\\test\\tree\\dest";
#endif
}
