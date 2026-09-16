#pragma once

#include <string>
#include "TestPaths.h"

namespace SyncSshTwoWayTests
{
	const std::wstring SOURCE_PATH = TestPaths::scratch_dir() + L"/two_way_ssh/source";
	const std::wstring TARGET_PATH = TestPaths::scratch_dir() + L"/two_way_ssh/target";
	const std::string REMOTE_DIR_NAME = "afs_two_way_ssh";  // [SyncSshTwoWayTests.md#remote-dir-per-test]
}
