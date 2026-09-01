cmake_minimum_required(VERSION 3.16)

# Must be known before the first project() call to take effect.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS version")
endif()

project(autofilesync LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AFS_BUILD_TESTS "Build the integration tests (AFS_Core) and GUI tests (AFS_Desktop)" OFF)

# All programs land directly in the build directory (build/autofilesync,
# build/Release/autofilesync ...): the scripts, IDE configurations and install
# instructions address them there, and the help book is copied next to them.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(AFS_BUILD_TESTS)
    enable_testing()
endif()

# AFS_Core: synchronisation engine, GPLv3. Builds without AFS_Desktop.
add_subdirectory(AFS_Core)
# AFS_Desktop: the wxWidgets application "autofilesync" on top of AFS_Core.
add_subdirectory(AFS_Desktop)
