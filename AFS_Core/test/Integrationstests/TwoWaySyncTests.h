#pragma once

#include <string>
#include "TestPaths.h"

namespace TwoWaySyncTests
{
	const std::wstring SOURCE_PATH = TestPaths::scratch_dir() + L"/two_way/source";  // [TwoWaySyncTests.md#own-scratch-paths]
	const std::wstring TARGET_PATH = TestPaths::scratch_dir() + L"/two_way/target";
}
