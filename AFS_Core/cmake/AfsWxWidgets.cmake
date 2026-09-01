# Provides the INTERFACE target afs_wxwidgets: wxWidgets 3.2 with the
# components the project uses, as a link target instead of the directory-wide
# flags of UsewxWidgets.cmake, so that it propagates from afs_core to every
# consumer. (CMake's own wxWidgets::wxWidgets target needs 3.27.)
include_guard(DIRECTORY)

if(NOT TARGET afs_wxwidgets)
    if(APPLE AND NOT wxWidgets_CONFIG_EXECUTABLE AND EXISTS /opt/homebrew/bin/wx-config-3.2)
        set(wxWidgets_CONFIG_EXECUTABLE /opt/homebrew/bin/wx-config-3.2)
    endif()
    find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base aui html)

    add_library(afs_wxwidgets INTERFACE)
    target_include_directories(afs_wxwidgets SYSTEM INTERFACE ${wxWidgets_INCLUDE_DIRS})
    target_compile_options(afs_wxwidgets INTERFACE ${wxWidgets_CXX_FLAGS})
    target_compile_definitions(afs_wxwidgets INTERFACE ${wxWidgets_DEFINITIONS} wxUSE_DATEPICKCTRL_GENERIC=1)
    foreach(definition IN LISTS wxWidgets_DEFINITIONS_DEBUG)
        target_compile_definitions(afs_wxwidgets INTERFACE $<$<CONFIG:Debug>:${definition}>)
    endforeach()
    target_link_directories(afs_wxwidgets INTERFACE ${wxWidgets_LIBRARY_DIRS})
    target_link_libraries(afs_wxwidgets INTERFACE ${wxWidgets_LIBRARIES})
endif()
