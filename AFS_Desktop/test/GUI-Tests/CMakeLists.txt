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
# UserActions types through XTest with the keys of the server's current
# keyboard layout (XKB) on Linux; macOS uses its own branch there and has no
# X server. The header is checked directly because find_package keeps the
# cached path even after the package has been uninstalled (e.g. by autoremove).
if(NOT APPLE)
    find_package(X11 REQUIRED)
    if(NOT X11_Xtst_FOUND OR NOT EXISTS "${X11_Xtst_INCLUDE_PATH}/X11/extensions/XTest.h")
        message(FATAL_ERROR "libXtst (XTest extension) is required to type in the GUI tests - "
                            "Debian/Ubuntu: sudo apt install libxtst-dev")
    endif()
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
    ViewActions.cpp
    SshTestServer.cpp
    AddSyncItemTest.cpp
    BackgroundServiceMenuTest.cpp
    AddSshAccountTest.cpp
    SettingsRecycleBinTest.cpp
    ChatLogQuestionTest.cpp
    TreeViewContextMenuTest.cpp
    TreeViewCrossSelectionTest.cpp
    SshDirDialogTest.cpp
    SyncOnceTest.cpp
    PreviewTest.cpp
    ContinuousSyncTest.cpp
    ProgressDialogTest.cpp
    HugeFileTest.cpp
    SyncOptionsTest.cpp
    SyncItemManagementTest.cpp
    SyncItemDialogRulesTest.cpp
    ViewsTest.cpp
    ConfigFileTest.cpp
    SettingsTest.cpp
    LoggingTest.cpp
    SshAccountsTest.cpp
    SshSyncTest.cpp
    WindowAndHelpTest.cpp
    ChatDialogTest.cpp
    SyncMessagesTest.cpp
)

# The complete application under test - the same source list as the shipped
# executable, so the tests never run against a reduced GUI. _GUITEST replaces
# the application's main() by the one of the test binary; see App.cpp.
add_executable(GuiTests ${TEST_SOURCES} ${AFS_DESKTOP_SOURCES})
afs_desktop_configure_target(GuiTests)
target_compile_definitions(GuiTests PRIVATE _GUITEST)
target_include_directories(GuiTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GuiTests PRIVATE GTest::GTest)
# Only libX11 and libXtst - ${X11_LIBRARIES} would also pull in libSM/libICE,
# whose cached paths survive the removal of their -dev packages.
if(NOT APPLE)
    target_link_libraries(GuiTests PRIVATE X11::X11 X11::Xtst)
endif()
if(APPLE)
    # UserActions injects keystrokes as CGEvents (layout independent);
    # ForegroundApp activates the process through AppKit.
    target_sources(GuiTests PRIVATE ForegroundApp.mm)
    target_link_libraries(GuiTests PRIVATE "-framework ApplicationServices" "-framework AppKit")
endif()
# The tests compare dialog texts with the same catalogue the application reads,
# so a build of the test binary alone must not leave a stale catalogue behind.
if(TARGET translations)
    add_dependencies(GuiTests translations)
endif()
# The application looks for the help books next to the executable, one per
# language (MainWindow.cpp, locate_help_book). help_book (AFS_Desktop/CMakeLists.txt)
# packs them; copied here, next to GuiTests, because help_book copies them only
# next to autofilesync, which on macOS is a bundle and on multi-config
# generators another directory.
if(TARGET help_book)
    add_dependencies(GuiTests help_book)
    add_custom_command(TARGET GuiTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/../../help $<TARGET_FILE_DIR:GuiTests>/help
        COMMENT "Copying the help books next to GuiTests")
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
