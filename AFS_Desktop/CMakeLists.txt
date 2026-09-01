cmake_minimum_required(VERSION 3.16)
project(AFS_Desktop LANGUAGES CXX)

option(AFS_BUILD_TESTS "Build the GUI tests" OFF)

# Standalone build: pull the core in ourselves. It must carry its wxWidgets
# hooks, which is what the application talks to.
if(NOT TARGET afs_core)
    set(AFS_CORE_WITH_WXWIDGETS ON)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../AFS_Core AFS_Core)
endif()

include(cmake/AfsDesktop.cmake)

set(SOURCES ${AFS_DESKTOP_SOURCES})
if(APPLE)
    # Listed as a source so CMake copies it into the bundle; CFBundleIconFile in
    # Info.plist names it without the extension.
    set(MACOSX_BUNDLE_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icons/afs.icns)
    set_source_files_properties(${MACOSX_BUNDLE_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND SOURCES ${MACOSX_BUNDLE_ICON})
endif()

add_executable(autofilesync ${SOURCES})
afs_desktop_configure_target(autofilesync)

if(APPLE)
    set_target_properties(autofilesync PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        MACOSX_BUNDLE_BUNDLE_NAME "AutoFileSync"
        MACOSX_BUNDLE_GUI_IDENTIFIER "de.autofilesync.app"
    )
    # Xcode runs the debug target with the environment Xcode itself was started
    # in, so a model path published later via launchctl stays invisible until
    # Xcode is restarted. Baking it into the generated scheme keeps debug runs
    # working; AUTOFILESYNC_LLAMA_MODEL is read at configure time because the
    # path is specific to the developer's machine.
    if(CMAKE_GENERATOR STREQUAL "Xcode" AND DEFINED ENV{AUTOFILESYNC_LLAMA_MODEL})
        set_target_properties(autofilesync PROPERTIES
            XCODE_GENERATE_SCHEME TRUE
            XCODE_SCHEME_ENVIRONMENT "AUTOFILESYNC_LLAMA_MODEL=$ENV{AUTOFILESYNC_LLAMA_MODEL}")
    endif()
endif()

# The program looks for its help book next to the executable (or among the
# bundle's resources on macOS); copying it there makes a build tree usable
# without an installation - the same thing the Windows post-build step does.
if(APPLE)
    set(HELP_DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:autofilesync>/Resources/help)
else()
    set(HELP_DESTINATION $<TARGET_FILE_DIR:autofilesync>/help)
endif()
add_custom_command(TARGET autofilesync POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/help ${HELP_DESTINATION}
    COMMENT "Copying the help book next to autofilesync")

if(AFS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test/GUI-Tests)
endif()
