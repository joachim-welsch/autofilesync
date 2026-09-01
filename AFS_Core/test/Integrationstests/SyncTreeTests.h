#pragma once

#include <string>
#include "TestPaths.h"

namespace SyncTreeTests
{
#ifdef _UNIX
	const std::wstring SourcePath = TestPaths::repoDir() + L"/test/resources/tree/";
	const std::wstring TargetPath = TestPaths::scratchDir() + L"/tree";
	const std::wstring ResourcePath = TestPaths::repoDir() + L"/test/tree/source/";
	const std::wstring CountTestSourcePath = TestPaths::scratchDir() + L"/tree/src/";
	const std::wstring CountTestTargetPath = TestPaths::scratchDir() + L"/tree/dest";
#else
	const std::wstring SourcePath = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\resources\\tree\\";
	const std::wstring TargetPath = L"C:\\temp\\test\\tree";
	const std::wstring ResourcePath = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\tree\\source\\";
	const std::wstring CountTestSourcePath = L"C:\\temp\\test\\tree\\src\\";
	const std::wstring CountTestTargetPath = L"C:\\temp\\test\\tree\\dest";
#endif
}
