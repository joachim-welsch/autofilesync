cmake_minimum_required(VERSION 3.16)
project(backgroundsync LANGUAGES CXX)

# The daemon is headless: it never links AFS_Desktop, so it cannot use an
# afs_core built with the wxWidgets hooks (SyncFile.cpp's copy-progress code
# would then need AFS_Desktop's ProgressDialog implementation). Build our own
# non-WXWIDGETS afs_core_headless from the same sources instead of reusing
# whatever afs_core the rest of this tree configured.
include(${CMAKE_CURRENT_SOURCE_DIR}/../AFS_Core/cmake/AfsCore.cmake)
if(NOT TARGET afs_core_headless)
    afs_core_add_library(afs_core_headless)
endif()

add_executable(${PROJECT_NAME}
    backgroundsync.cpp
    MyDaemon.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE afs_core_headless)
