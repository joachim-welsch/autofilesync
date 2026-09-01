#pragma once
#include <string>
#include "TestPaths.h"

namespace SyncFileTests
{
    const std::wstring SourceFilename = L"test1.txt";
    const std::wstring LargerSourceFilename = L"test1_larger.txt";
    const std::wstring TargetFile = L"test1.txt";
#ifdef _UNIX
    const std::wstring ResourcePath = TestPaths::repoDir() + L"/test/resources/files/";
    const std::wstring SourcePath = TestPaths::scratchDir() + L"/source/";
    const std::wstring TargetDir = TestPaths::scratchDir() + L"/dest/";
    const std::wstring TargetPathName = TargetDir + TargetFile;
    const std::wstring LargerTargetPathName = TargetDir + LargerSourceFilename;
#else
    const std::wstring ResourcePath = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\resources\\files\\";
    const std::wstring SourcePath = L"C:\\temp\\test\\source\\";
    const std::wstring TargetDir = L"C:\\temp\\test\\dest\\";
    const std::wstring TargetPathName = TargetDir + TargetFile;
    const std::wstring LargerTargetPathName = TargetDir + L"\\" + LargerSourceFilename;
#endif

    const std::wstring ResourcePathName = ResourcePath + SourceFilename;
    const std::wstring LargerResourcePathName = ResourcePath + LargerSourceFilename;
    const std::wstring SourcePathName = SourcePath + SourceFilename;
    const std::wstring LargerSourcePathName = SourcePath + LargerSourceFilename;

}

