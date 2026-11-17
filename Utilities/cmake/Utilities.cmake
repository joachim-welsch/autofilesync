# Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of Utilities, the general-purpose helpers of Autofilesync.
#
# Utilities is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# Utilities is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# Utilities. If not, see <https://www.gnu.org/licenses/>.
# The general-purpose modules that know nothing about synchronisation. They are
# no library of their own: afs_core_add_library() (AFS_Core/cmake/AfsCore.cmake)
# compiles UTILITIES_SOURCES into every build of the core, so they see the same
# preprocessor definitions (_UNIX, _MACOS) and the precompiled header pch.h of
# the consuming project. Nothing in here may include a header of AFS_Core,
# AFS_Desktop or backgroundsync - except the two every consuming project has to
# supply: pch.h and UtilitiesConfig.h, which names the program (log directory,
# gettext text domain) and decides how the namespace Utilities is seen there. Translation.cpp reads the compile definitions
# TRANSLATION_WITH_GETTEXT and TRANSLATION_CATALOGUE_STAMP; the message ids the
# modules translate have to be in the catalogue of the program.
include_guard(DIRECTORY)

get_filename_component(UTILITIES_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

set(UTILITIES_SOURCES
    StrUtil.cpp
    Base64.cpp
    Hex.cpp
    PercentEncoding.cpp
    FileTime.cpp
    FileName.cpp
    SystemInfo.cpp
    JsonText.cpp
    CpuTopology.cpp
    Logging.cpp
    LogEmail.cpp
    SmtpMailer.cpp
    LogCut.cpp
    Translation.cpp
    MountedDrives.cpp
    FilesystemUtil.cpp
    SymlinkUtil.cpp
    RecycleBin.cpp
)
list(TRANSFORM UTILITIES_SOURCES PREPEND ${UTILITIES_DIR}/src/)

# Compiled into the test program of the consuming project
# (AFS_Core/test/Integrationstests), which supplies main(), pch.h with Google
# Test and the helpers the tests include (ScopedLog.h, ScopedEnv.h, ScopedFstab.h,
# SmtpSink.h).
set(UTILITIES_TEST_SOURCES
    StrUtilTests.cpp
    Base64Tests.cpp
    HexTests.cpp
    PercentEncodingTests.cpp
    FileTimeTests.cpp
    FileNameTests.cpp
    SystemInfoTests.cpp
    JsonTextTests.cpp
    CpuTopologyTests.cpp
    LoggingCollectTests.cpp
    LoggingConcurrencyTests.cpp
    SmtpMailerTests.cpp
    LogCutTests.cpp
    TranslationTests.cpp
    FilesystemUtilTests.cpp
    SymlinkUtilTests.cpp
    RecycleBinTests.cpp
    MountedDrivesTests.cpp
    ThreadSaveStackTests.cpp
)
list(TRANSFORM UTILITIES_TEST_SOURCES PREPEND ${UTILITIES_DIR}/test/)
