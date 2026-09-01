#pragma once
#include <string>
#include "TestPaths.h"

namespace LoggingTests
{
	const std::wstring SourceFilename = L"test1.txt";
	const std::wstring TargetFile = L"test1.txt";

#ifdef _UNIX
	const std::wstring SourcePath = TestPaths::scratchDir() + L"/source/";
	const std::wstring TargetDir = TestPaths::scratchDir() + L"/dest/";
	const std::wstring LoggingPath = TestPaths::scratchDir() + L"/afs_log.txt";
#else
	const std::wstring NotMountedSourcePath = L"Y:\\";
	const std::wstring TargetDir = L"C:\\temp\\test\\dest\\";
	const std::wstring LoggingPath = L"C:\\temp\\test\\afs_log.txt";
#endif

	const std::wstring NotMountedSourcePathName = NotMountedSourcePath + SourceFilename;

}