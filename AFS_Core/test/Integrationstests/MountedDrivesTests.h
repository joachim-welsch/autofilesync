#pragma once
#include <string>
#include "TestPaths.h"

namespace MountedDrivesTests {
#ifdef _UNIX
    const std::wstring TESTMOUNT = L"/media/winuser/test";
    const std::wstring TESTMOUNTFN = TestPaths::scratch_dir() + L"/test1.txt";
    const std::wstring TESTMOUNT2 = L"/";
#else
	const std::wstring TESTMOUNT = L"Z:\\";
	const std::wstring TESTMOUNT2 = L"C:\\";
	const std::wstring TESTMOUNTFN = L"C:\\temp\\test\\test1.txt";
#endif
}

