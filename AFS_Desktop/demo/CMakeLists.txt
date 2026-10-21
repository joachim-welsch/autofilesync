cmake_minimum_required(VERSION 3.16)

# afs_demo: drives the complete application through the scenes of the
# introduction video while a screen recorder watches. Linux and macOS only -
# it reuses the input simulation of the GUI tests (test/GUI-Tests). Builds
# standalone (cmake -S AFS_Desktop/demo) or with -DAFS_BUILD_DEMO=ON.
project(AfsDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET afs_core)
    set(AFS_CORE_WITH_WXWIDGETS ON)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../AFS_Core AFS_Core)
endif()
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/AfsDesktop.cmake)

# The harness sources report through Google Test's ADD_FAILURE, so the demo
# links it although it runs no tests.
find_package(GTest REQUIRED)
if(NOT APPLE)
    find_package(X11 REQUIRED)
    if(NOT X11_Xtst_FOUND OR NOT EXISTS "${X11_Xtst_INCLUDE_PATH}/X11/extensions/XTest.h")
        message(FATAL_ERROR "libXtst (XTest extension) is required to drive the demo - Debian/Ubuntu: sudo apt install libxtst-dev")
    endif()
endif()

set(HARNESS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test/GUI-Tests)
set(HARNESS_SOURCES
    EventPump.cpp
    UserActions.cpp
    ModalDialogWatcher.cpp
    SyncActions.cpp
    SyncItemDialogDriver.cpp
)
list(TRANSFORM HARNESS_SOURCES PREPEND ${HARNESS_DIR}/)

set(DEMO_SOURCES
    DemoMain.cpp
    DemoSandbox.cpp
    Options.cpp
    Pacing.cpp
    Recording.cpp
    Scenes.cpp
)

# _GUITEST: App.cpp leaves main() to DemoMain.cpp, exactly as for the GUI tests.
add_executable(afs_demo ${DEMO_SOURCES} ${HARNESS_SOURCES} ${AFS_DESKTOP_SOURCES})
afs_desktop_configure_target(afs_demo)
target_compile_definitions(afs_demo PRIVATE _GUITEST)
target_include_directories(afs_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${HARNESS_DIR})
target_link_libraries(afs_demo PRIVATE GTest::GTest)
if(NOT APPLE)
    # Pacing.cpp asks GTK for the toolbar item rectangles (wxGTK has no API for
    # them) through its own declarations, so only the library is needed, not
    # the GTK headers.
    find_library(GTK3_LIBRARY NAMES gtk-3 libgtk-3.so.0 REQUIRED)
    target_link_libraries(afs_demo PRIVATE X11::X11 X11::Xtst ${GTK3_LIBRARY})
endif()
if(APPLE)
    target_sources(afs_demo PRIVATE ${HARNESS_DIR}/ForegroundApp.mm)
    target_link_libraries(afs_demo PRIVATE "-framework ApplicationServices" "-framework AppKit")
endif()
if(TARGET translations)
    add_dependencies(afs_demo translations)
endif()
if(TARGET help_book)
    add_dependencies(afs_demo help_book)
    add_custom_command(TARGET afs_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/../help $<TARGET_FILE_DIR:afs_demo>/help
        COMMENT "Copying the help books next to afs_demo")
endif()
