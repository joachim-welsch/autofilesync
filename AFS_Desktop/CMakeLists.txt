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

# The program loads only the packed book help.htb, not the pages, so the build
# packs it from the pages listed in help.hhp (plus contents, index, project file
# and context map) - the same thing tools/packHelp.* does for the MSBuild build.
# The book lands next to the pages, where every packaging script expects it.
set(HELP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/help)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HELP_SOURCE_DIR}/help.hhp)
file(STRINGS ${HELP_SOURCE_DIR}/help.hhp HELP_PROJECT_LINES)
set(HELP_BOOK_FILES "")
set(in_files_section FALSE)
foreach(line IN LISTS HELP_PROJECT_LINES)
    if(line MATCHES "^\\[FILES\\]")
        set(in_files_section TRUE)
    elseif(line MATCHES "^\\[")
        set(in_files_section FALSE)
    elseif(in_files_section AND NOT line MATCHES "^[ \t]*$")
        string(STRIP "${line}" page)
        list(APPEND HELP_BOOK_FILES ${page})
    endif()
endforeach()
list(APPEND HELP_BOOK_FILES contents.hhc index.hhk help.hhp wxhelp.map)
list(TRANSFORM HELP_BOOK_FILES PREPEND ${HELP_SOURCE_DIR}/ OUTPUT_VARIABLE HELP_BOOK_DEPENDS)
set(HELP_BOOK ${HELP_SOURCE_DIR}/help.htb)
add_custom_command(OUTPUT ${HELP_BOOK}
    COMMAND ${CMAKE_COMMAND} -E tar cf ${HELP_BOOK} --format=zip -- ${HELP_BOOK_FILES}
    WORKING_DIRECTORY ${HELP_SOURCE_DIR}
    DEPENDS ${HELP_BOOK_DEPENDS}
    COMMENT "Packing the help book")
add_custom_target(help_book DEPENDS ${HELP_BOOK})
add_dependencies(autofilesync help_book)

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

# Every package carries the licence texts; the Windows and Debian packages add
# them themselves, the macOS bundle takes them among its resources.
if(APPLE)
    set(LICENSE_DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:autofilesync>/Resources/licenses)
    add_custom_command(TARGET autofilesync POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LICENSE_DESTINATION}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/LICENSING.md
            ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md ${CMAKE_SOURCE_DIR}/CHANGELOG.md ${LICENSE_DESTINATION}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/third-party ${LICENSE_DESTINATION}/third-party
        COMMENT "Copying the licence texts into the bundle")
endif()

# The message catalogues are built into <build>/locale, which on Linux and
# Windows is already the directory of the executable; the macOS bundle needs
# them among its resources, just like the help book.
if(TARGET translations)
    add_dependencies(autofilesync translations)
    if(APPLE)
        add_custom_command(TARGET autofilesync POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/locale $<TARGET_BUNDLE_CONTENT_DIR:autofilesync>/Resources/locale
            COMMENT "Copying the message catalogues into the bundle")
    endif()
endif()

if(AFS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test/GUI-Tests)
endif()
