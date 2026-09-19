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
# Provides the INTERFACE target afs_wxwidgets: wxWidgets 3.2 with the
# components the project uses, as a link target instead of the directory-wide
# flags of UsewxWidgets.cmake, so that it propagates from afs_core to every
# consumer. (CMake's own wxWidgets::wxWidgets target needs 3.27.)
include_guard(DIRECTORY)

if(NOT TARGET afs_wxwidgets)
    if(APPLE AND NOT wxWidgets_CONFIG_EXECUTABLE AND EXISTS /opt/homebrew/bin/wx-config-3.2)
        set(wxWidgets_CONFIG_EXECUTABLE /opt/homebrew/bin/wx-config-3.2)
    endif()
    find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base aui html)

    add_library(afs_wxwidgets INTERFACE)
    target_include_directories(afs_wxwidgets SYSTEM INTERFACE ${wxWidgets_INCLUDE_DIRS})
    target_compile_options(afs_wxwidgets INTERFACE ${wxWidgets_CXX_FLAGS})
    target_compile_definitions(afs_wxwidgets INTERFACE ${wxWidgets_DEFINITIONS} wxUSE_DATEPICKCTRL_GENERIC=1)
    foreach(definition IN LISTS wxWidgets_DEFINITIONS_DEBUG)
        target_compile_definitions(afs_wxwidgets INTERFACE $<$<CONFIG:Debug>:${definition}>)
    endforeach()
    target_link_directories(afs_wxwidgets INTERFACE ${wxWidgets_LIBRARY_DIRS})
    target_link_libraries(afs_wxwidgets INTERFACE ${wxWidgets_LIBRARIES})
endif()
