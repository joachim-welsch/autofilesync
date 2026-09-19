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
project(AFS_Core LANGUAGES CXX)

# The core still carries a few wxWidgets hooks (Global::getApp, message boxes
# in Config.cpp, the progress dialog in SyncFile.cpp), all behind _WXWIDGETS.
# AFS_Desktop needs them; a headless consumer or the tests build without.
option(AFS_CORE_WITH_WXWIDGETS "Compile the wxWidgets hooks of the core (needed by AFS_Desktop)" ON)
option(AFS_BUILD_TESTS "Build the integration tests" OFF)

include(cmake/AfsCore.cmake)

if(AFS_CORE_WITH_WXWIDGETS)
    afs_core_add_library(afs_core WXWIDGETS)
else()
    afs_core_add_library(afs_core)
endif()

if(AFS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test/Integrationstests)
endif()
