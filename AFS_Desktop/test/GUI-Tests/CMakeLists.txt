cmake_minimum_required(VERSION 3.16)

# Build for Linux and macOS; Windows uses GuiTests.vcxproj. Builds standalone
# (cmake -S AFS_Desktop/test/GUI-Tests) or as part of the top-level project
# with -DAFS_BUILD_TESTS=ON.
project(GuiTestProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if(NOT TARGET afs_core)
    set(AFS_CORE_WITH_WXWIDGETS ON)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../AFS_Core AFS_Core)
endif()
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/AfsDesktop.cmake)

find_package(GTest REQUIRED)
# UserActions reads the active keyboard layout to type shifted characters -
# through X on Linux; macOS uses its own branch there and has no X server.
if(NOT APPLE)
    find_package(X11 REQUIRED)
endif()

enable_testing()

set(TEST_SOURCES
    GuiTestMain.cpp
    GuiTest.cpp
    GuiTestSandbox.cpp
    EventPump.cpp
    UserActions.cpp
    ModalDialogWatcher.cpp
    SyncItemDialogDriver.cpp
    SshAccountDialogDriver.cpp
    SettingsDialogDriver.cpp
    LoggingDialogDriver.cpp
    SandboxTree.cpp
    SyncActions.cpp
    AddSyncItemTest.cpp
    BackgroundServiceMenuTest.cpp
    AddSshAccountTest.cpp
    SettingsRecycleBinTest.cpp
    ChatLogQuestionTest.cpp
    TreeViewContextMenuTest.cpp
    TreeViewCrossSelectionTest.cpp
    SshDirDialogTest.cpp
    SyncOnceTest.cpp
)

# The complete application under test - the same source list as the shipped
# executable, so the tests never run against a reduced GUI. _GUITEST replaces
# the application's main() by the one of the test binary; see App.cpp.
add_executable(GuiTests ${TEST_SOURCES} ${AFS_DESKTOP_SOURCES})
afs_desktop_configure_target(GuiTests)
target_compile_definitions(GuiTests PRIVATE _GUITEST)
target_include_directories(GuiTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${X11_INCLUDE_DIR})
target_link_libraries(GuiTests PRIVATE GTest::GTest ${X11_LIBRARIES})
if(APPLE)
    # UserActions injects keystrokes as CGEvents (layout independent).
    target_link_libraries(GuiTests PRIVATE "-framework ApplicationServices")
endif()

# Always through the wrapper: it provides the virtual X display the tests need
# and keeps them off the developer's own screen. CROSSCOMPILING_EMULATOR makes
# gtest_discover_tests prepend it to both the discovery run and every
# generated test invocation - run_gui_tests.sh forwards its own arguments
# ($1 = binary, rest = gtest args) straight through to GuiTests.
set_target_properties(GuiTests PROPERTIES CROSSCOMPILING_EMULATOR "${CMAKE_CURRENT_SOURCE_DIR}/run_gui_tests.sh")

# Register each TEST() case individually with CTest instead of the binary as
# a whole. DISCOVERY_MODE PRE_TEST: discovery starts the real wxWidgets app
# and takes over the desktop's mouse/keyboard, so that should only happen on
# an explicit ctest run, not on every build. RUN_SERIAL: multiple processes
# must not compete for the same real desktop at once.
include(GoogleTest)
gtest_discover_tests(GuiTests DISCOVERY_MODE PRE_TEST PROPERTIES RUN_SERIAL TRUE)
