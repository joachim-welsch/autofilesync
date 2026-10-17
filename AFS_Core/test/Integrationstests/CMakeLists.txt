# Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of AFS_Core, the synchronisation engine of Autofilesync.
#
# AFS_Core is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# AFS_Core. If not, see <https://www.gnu.org/licenses/>.
cmake_minimum_required(VERSION 3.16)

# Builds standalone (cmake -S AFS_Core/test/Integrationstests) or as part of the
# top-level project with -DAFS_BUILD_TESTS=ON.
project(IntegrationtestProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/AfsCore.cmake)
find_package(GTest REQUIRED)

enable_testing()

# _TEST opens up Synchronize, SyncFile and SyncFileSsh for the tests and changes
# their class layouts, so the tests get their own build of the core instead of
# the library the application links. No wxWidgets hooks here.
afs_core_add_library(afs_core_test DEFINITIONS _TEST)

set(SOURCES
    pch.cpp
    FileUtil.cpp
    DirUtil.cpp
    SmtpSink.cpp
    SyncFileTests.cpp
    SyncDirTests.cpp
    DifficultFilenameTests.cpp
    DifficultDirnameTests.cpp
    SyncItemTests.cpp
    ConfigTests.cpp
    CopyLogTests.cpp
    FileDeleteTests.cpp
    PasswordCryptoTests.cpp
    StrUtilTests.cpp
    TranslationTests.cpp
    JsonTextTests.cpp
    LlamaClientTests.cpp
    HelpKnowledgeTests.cpp
    ScopedEnv.cpp
    UtilTests.cpp
    FilesystemUtilTests.cpp
    RecycleBinTests.cpp
    FileDifferenceTests.cpp
    LoggingConcurrencyTests.cpp
    Libssh2InitTests.cpp
    SyncTreeTests.cpp
    TwoWaySyncTests.cpp
    SyncFlagTests.cpp
    LocalPermissionTests.cpp
    SymlinkTests.cpp
    MountedDrivesTests.cpp
    SynchronizeTests.cpp
    SyncArbiterTests.cpp
    CheckPathTests.cpp
    LogCutTests.cpp
    LoggingCollectTests.cpp
    LogAnalysisTests.cpp
    ModelLocationTests.cpp
    CpuTopologyTests.cpp
    SyncProposalTests.cpp
    SyncPlanTests.cpp
    ConfigSummaryTests.cpp
    SyncSshFileTests.cpp
    SyncSshDirTests.cpp
    SyncSshTreeTests.cpp
    SyncSshTwoWayTests.cpp
    SyncSshToSshTests.cpp
    SshPathEncodingTests.cpp
    SshHostKeyTests.cpp
    SshConnectTimeoutTests.cpp
    SshDirInspectorTests.cpp
    SmtpMailerTests.cpp
    SshTest.cpp
    TestMain.cpp
)

add_executable(Integrationtests ${SOURCES})
target_include_directories(Integrationtests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Integrationtests PRIVATE afs_core_test GTest::GTest)
# Root of AFS_Core: the fixtures find test/resources and test/tree below it.
target_compile_definitions(Integrationtests PRIVATE LIB_SOURCE_DIR="${AFS_CORE_DIR}")

# Register each TEST() case individually with CTest instead of the binary as a
# whole. DISCOVERY_MODE PRE_TEST: the default POST_BUILD runs the binary from a
# script phase, which the Xcode generator schedules before code signing - macOS
# kills an unsigned binary at once. RUN_SERIAL: the tests share fixed rather
# than random directory paths and would disturb each other under real ctest
# parallelism.
include(GoogleTest)
gtest_discover_tests(Integrationtests DISCOVERY_MODE PRE_TEST PROPERTIES RUN_SERIAL TRUE)
