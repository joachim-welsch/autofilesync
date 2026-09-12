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

# Message catalogues: msgfmt turns po/<language>.po into the layout gettext
# looks for next to the executable (locale/<language>/LC_MESSAGES/autofilesync.mo).
# Without msgfmt the programs show the bare message ids: the ids are technical
# keys, so there is no language left to fall back on. msgfmt is not required the
# way libintl is (see AfsCore.cmake) - a missing catalogue is a packaging fault,
# a missing libintl would be compiled-in.
# Defined before the subdirectories so that AFS_Desktop can depend on the target.
find_program(MSGFMT_EXECUTABLE msgfmt)
if(MSGFMT_EXECUTABLE)
    file(GLOB AFS_PO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/po/*.po)
    foreach(po_file ${AFS_PO_FILES})
        get_filename_component(language ${po_file} NAME_WE)
        set(mo_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/locale/${language}/LC_MESSAGES)
        add_custom_command(OUTPUT ${mo_dir}/autofilesync.mo
            COMMAND ${CMAKE_COMMAND} -E make_directory ${mo_dir}
            COMMAND ${MSGFMT_EXECUTABLE} -o ${mo_dir}/autofilesync.mo ${po_file}
            DEPENDS ${po_file}
            COMMENT "Compiling the message catalogue ${language}")
        list(APPEND AFS_MO_FILES ${mo_dir}/autofilesync.mo)
    endforeach()
    add_custom_target(translations ALL DEPENDS ${AFS_MO_FILES})
endif()

# AFS_Core: synchronisation engine, GPLv3. Builds without AFS_Desktop.
add_subdirectory(AFS_Core)
# AFS_Desktop: the wxWidgets application "autofilesync" on top of AFS_Core.
add_subdirectory(AFS_Desktop)
# backgroundsync: headless daemon running the same sync engine as the GUI.
# Binary dir renamed: the target is also called "backgroundsync", which would
# otherwise collide with this directory under CMAKE_RUNTIME_OUTPUT_DIRECTORY.
add_subdirectory(backgroundsync backgroundsync_build)

# Xcode offers only targets that have a scheme, and stops autocreating schemes
# as soon as the project carries one - AFS_Desktop's scheme for the model path
# would otherwise hide every other target. Name the runnable ones explicitly.
if(CMAKE_GENERATOR STREQUAL "Xcode")
    foreach(afs_scheme_target autofilesync backgroundsync Integrationtests GuiTests)
        if(TARGET ${afs_scheme_target})
            set_target_properties(${afs_scheme_target} PROPERTIES XCODE_GENERATE_SCHEME TRUE)
        endif()
    endforeach()
endif()
