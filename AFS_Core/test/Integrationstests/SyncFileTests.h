/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
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
    const std::wstring RESOURCE_PATH = L"C:\\Entwicklung\\autofilesync\\AFS_Core\\test\\resources\\files\\";
    const std::wstring SOURCE_PATH = L"C:\\temp\\test\\source\\";
    const std::wstring TARGET_DIR = L"C:\\temp\\test\\dest\\";
    const std::wstring TARGET_PATH_NAME = TARGET_DIR + TARGET_FILE;
    const std::wstring LARGER_TARGET_PATH_NAME = TARGET_DIR + LARGER_SOURCE_FILENAME;
#endif

    const std::wstring RESOURCE_PATH_NAME = RESOURCE_PATH + SOURCE_FILENAME;
    const std::wstring LARGER_RESOURCE_PATH_NAME = RESOURCE_PATH + LARGER_SOURCE_FILENAME;
    const std::wstring SOURCE_PATH_NAME = SOURCE_PATH + SOURCE_FILENAME;
    const std::wstring LARGER_SOURCE_PATH_NAME = SOURCE_PATH + LARGER_SOURCE_FILENAME;

}

