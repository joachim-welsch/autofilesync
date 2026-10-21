cmake_minimum_required(VERSION 3.16)
project(AFS_Desktop LANGUAGES CXX)

option(AFS_BUILD_TESTS "Build the GUI tests" OFF)
option(AFS_BUILD_DEMO "Build afs_demo, the scripted demo for the introduction video" OFF)

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
# One book per language: the German pages in help/, every other language in
# help/<language>/ - every subdirectory with a help.hhp is one. Each book lands next to its pages, where every packaging
# script expects it.
set(HELP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/help)
function(afs_pack_help_book directory)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${directory}/help.hhp)
    file(STRINGS ${directory}/help.hhp project_lines)
    set(files "")
    set(in_files_section FALSE)
    foreach(line IN LISTS project_lines)
        if(line MATCHES "^\\[FILES\\]")
            set(in_files_section TRUE)
        elseif(line MATCHES "^\\[")
            set(in_files_section FALSE)
        elseif(in_files_section AND NOT line MATCHES "^[ \t]*$")
            string(STRIP "${line}" page)
            list(APPEND files ${page})
        endif()
    endforeach()
    list(APPEND files contents.hhc index.hhk help.hhp wxhelp.map)
    foreach(toc IN ITEMS contents.hhc index.hhk)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${directory}/${toc})
        file(READ ${directory}/${toc} toc_text)
        if(toc_text MATCHES "value=\"[^\"]*&[a-zA-Z]+;")  # wxHtmlHelp shows such a name as an empty entry
            message(FATAL_ERROR "${directory}/${toc}: HTML entity in an entry name, write the character in UTF-8 instead")
        endif()
    endforeach()
    list(TRANSFORM files PREPEND ${directory}/ OUTPUT_VARIABLE depends)
    add_custom_command(OUTPUT ${directory}/help.htb
        COMMAND ${CMAKE_COMMAND} -E tar cf ${directory}/help.htb --format=zip -- ${files}
        WORKING_DIRECTORY ${directory}
        DEPENDS ${depends}
        COMMENT "Packing the help book in ${directory}")
    set_property(GLOBAL APPEND PROPERTY AFS_HELP_BOOKS ${directory}/help.htb)
endfunction()
afs_pack_help_book(${HELP_SOURCE_DIR})
file(GLOB HELP_LANGUAGE_DIRS LIST_DIRECTORIES true CONFIGURE_DEPENDS ${HELP_SOURCE_DIR}/*)
foreach(directory IN LISTS HELP_LANGUAGE_DIRS)
    if(IS_DIRECTORY ${directory} AND EXISTS ${directory}/help.hhp)
        afs_pack_help_book(${directory})
    endif()
endforeach()
get_property(HELP_BOOKS GLOBAL PROPERTY AFS_HELP_BOOKS)
add_custom_target(help_book DEPENDS ${HELP_BOOKS})
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
# A changed page repacks the books without relinking the program, so the books
# are copied after packing as well; otherwise the build tree showed the old help.
# Only where the program's directory is known without asking the target, which
# would make help_book depend on autofilesync and close a cycle.
if(NOT APPLE AND NOT CMAKE_CONFIGURATION_TYPES)
    add_custom_command(TARGET help_book POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/help ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/help
        COMMENT "Copying the help books next to autofilesync")
endif()

# Every package carries the licence texts; the Windows and Debian packages add
# them themselves, the macOS bundle takes them among its resources.
if(APPLE)
    set(LICENSE_DESTINATION $<TARGET_BUNDLE_CONTENT_DIR:autofilesync>/Resources/licenses)
    add_custom_command(TARGET autofilesync POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LICENSE_DESTINATION}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/doc/LICENSING.md
            ${CMAKE_SOURCE_DIR}/doc/THIRD-PARTY-NOTICES.md ${CMAKE_SOURCE_DIR}/doc/CHANGELOG.md ${LICENSE_DESTINATION}
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

if(AFS_BUILD_DEMO)
    add_subdirectory(demo)
endif()
