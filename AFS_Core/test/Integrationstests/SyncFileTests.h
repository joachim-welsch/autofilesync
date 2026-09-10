#pragma once
#include <string>
#include "TestPaths.h"

namespace SyncFileTests
{
    const std::wstring SOURCE_FILENAME = L"test1.txt";
    const std::wstring LARGER_SOURCE_FILENAME = L"test1_larger.txt";
    const std::wstring TARGET_FILE = L"test1.txt";
#ifdef _UNIX
    const std::wstring RESOURCE_PATH = TestPaths::repo_dir() + L"/test/resources/files/";
    const std::wstring SOURCE_PATH = TestPaths::scratch_dir() + L"/source/";
    const std::wstring TARGET_DIR = TestPaths::scratch_dir() + L"/dest/";
    const std::wstring TARGET_PATH_NAME = TARGET_DIR + TARGET_FILE;
    const std::wstring LARGER_TARGET_PATH_NAME = TARGET_DIR + LARGER_SOURCE_FILENAME;
#else
    const std::wstring ResourcePath = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\resources\\files\\";
    const std::wstring SourcePath = L"C:\\temp\\test\\source\\";
    const std::wstring TargetDir = L"C:\\temp\\test\\dest\\";
    const std::wstring TargetPathName = TargetDir + TargetFile;
    const std::wstring LargerTargetPathName = TargetDir + L"\\" + LargerSourceFilename;
#endif

    const std::wstring RESOURCE_PATH_NAME = RESOURCE_PATH + SOURCE_FILENAME;
    const std::wstring LARGER_RESOURCE_PATH_NAME = RESOURCE_PATH + LARGER_SOURCE_FILENAME;
    const std::wstring SOURCE_PATH_NAME = SOURCE_PATH + SOURCE_FILENAME;
    const std::wstring LARGER_SOURCE_PATH_NAME = SOURCE_PATH + LARGER_SOURCE_FILENAME;

}

