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
