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
# AFS_CATALOGUE_STAMP ties the compiled message catalogues to the message ids
# the program was built with: the SHA-1 of po/autofilesync.pot, the list of all
# ids. The programs carry it as a compile definition (AfsCore.cmake), the
# catalogues as the extra entry "catalogue_stamp" (StampCatalogue.cmake), and
# translation::catalogue_mismatch() compares the two at startup - a program
# copied over an installation whose catalogues were left behind otherwise shows
# bare ids and feeds them to the assistant without anyone noticing.
# A changed .pot reconfigures, so both sides always see the same value.
include_guard(DIRECTORY)
get_filename_component(AFS_POT_FILE ${CMAKE_CURRENT_LIST_DIR}/../../po/autofilesync.pot ABSOLUTE)
file(SHA1 ${AFS_POT_FILE} AFS_CATALOGUE_STAMP)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${AFS_POT_FILE})
