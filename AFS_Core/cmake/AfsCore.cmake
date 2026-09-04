# Everything needed to compile the AFS_Core sources: the source lists, the
# third-party dependencies and afs_core_add_library(), which turns them into a
# static library. The test projects include this file as well and compile the
# same sources a second time with their own preprocessor definitions (_TEST
# changes class layouts, so they cannot link the application's library).
#
# Variables are set per directory (include_guard(DIRECTORY)); the targets that
# hold the dependencies are global and only created once.
include_guard(DIRECTORY)

get_filename_component(AFS_CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

set(AFS_CORE_SOURCES
    BackupProof.cpp
    CheckPath.cpp
    Directory.cpp
    FileDelete.cpp
    FileDifference.cpp
    Global.cpp
    Libssh2Init.cpp
    LlamaClient.cpp
    LogAnalysis.cpp
    PasswordCrypto.cpp
    SshAccount.cpp
    SshConnectTimeout.cpp
    SshDirInspector.cpp
    SshFileInspector.cpp
    SshHostKeyVerification.cpp
    SyncArbiter.cpp
    SyncBase.cpp
    SyncControler.cpp
    SyncDir.cpp
    SyncFile.cpp
    SyncFileSsh.cpp
    SyncMessage.cpp
    SyncTree.cpp
    Synchronize.cpp
    Config.cpp
    Translation.cpp
    Settings.cpp
    SyncItem.cpp
    winutil.cpp
)
list(TRANSFORM AFS_CORE_SOURCES PREPEND ${AFS_CORE_DIR}/src/)

# Utility sources taken over from mylib; they live in the core so that AFS_Core
# is self-contained.
set(AFS_MYLIB_SOURCES
    FilesystemUtil.cpp
    LogCut.cpp
    Logging.cpp
    MountedDrives.cpp
    RecycleBin.cpp
    SmtpMailer.cpp
    strutil.cpp
    util.cpp
)
list(TRANSFORM AFS_MYLIB_SOURCES PREPEND ${AFS_CORE_DIR}/mylib/src/)

if(NOT TARGET afs_core_deps)
    # Boost is used header-only (property_tree, algorithm).
    find_package(Boost 1.83 REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSSH REQUIRED libssh)

    # GNU gettext for the translated texts (Translation.cpp). Part of the C library on
    # glibc, a separate libintl on macOS and Windows - optional, because without
    # it the programs still show the German message ids.
    find_package(Intl)

    # LlamaClient runs the assistant chat locally through llama.cpp instead of a
    # cloud API - pinned to a commit so the build stays reproducible. Only the
    # "llama" static library is built: the CLI tools, server and tests this repo
    # has no use for are switched off before the subproject configures.
    # Not a shallow clone: upstream only keeps recent "master-<hash>" tags around
    # and prunes older ones (confirmed the hard way - a pin from a few hours
    # earlier had already lost its tag), and a shallow "--no-single-branch --depth
    # 1" clone can only check out a commit that is still some branch's tip. A full
    # clone always has the pinned commit, whether or not a tag still points at it.
    # The one-time clone (cached under <build>/_deps) takes a couple of minutes
    # on a fresh build tree; GIT_PROGRESS/FETCHCONTENT_QUIET keep that first run
    # from looking like a hung "Configuring CMake...". An existing checkout can be
    # reused with -DFETCHCONTENT_SOURCE_DIR_LLAMA_CPP=<path>.
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_APP OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        llama_cpp
        GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
        GIT_TAG        d2f83055d6e3b379b5d34c4837122a918cf402c2
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(llama_cpp)

    add_library(afs_core_deps INTERFACE)
    target_include_directories(afs_core_deps SYSTEM INTERFACE ${Boost_INCLUDE_DIRS} ${LIBSSH_INCLUDE_DIRS})
    target_link_directories(afs_core_deps INTERFACE ${LIBSSH_LIBRARY_DIRS})
    target_link_libraries(afs_core_deps INTERFACE ${LIBSSH_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto llama Threads::Threads)
    if(Intl_FOUND)
        target_include_directories(afs_core_deps SYSTEM INTERFACE ${Intl_INCLUDE_DIRS})
        target_link_libraries(afs_core_deps INTERFACE ${Intl_LIBRARIES})
        target_compile_definitions(afs_core_deps INTERFACE AFS_WITH_GETTEXT)
    endif()
endif()

# afs_core_add_library(<target> [WXWIDGETS] [DEFINITIONS <def>...])
# Creates a static library from the core sources. WXWIDGETS compiles the
# wxWidgets hooks in (_WXWIDGETS) and links wxWidgets; DEFINITIONS are added as
# PUBLIC compile definitions so that consumers see the same class layouts.
function(afs_core_add_library target)
    cmake_parse_arguments(ARG "WXWIDGETS" "" "DEFINITIONS" ${ARGN})
    add_library(${target} STATIC ${AFS_CORE_SOURCES} ${AFS_MYLIB_SOURCES})
    target_include_directories(${target} PUBLIC ${AFS_CORE_DIR}/include ${AFS_CORE_DIR}/mylib/include)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PUBLIC ${ARG_DEFINITIONS})
    if(NOT WIN32)
        target_compile_definitions(${target} PUBLIC _UNIX)
    endif()
    if(APPLE)
        target_compile_definitions(${target} PUBLIC _MACOS)
    endif()
    target_link_libraries(${target} PUBLIC afs_core_deps)
    if(ARG_WXWIDGETS)
        include(${AFS_CORE_DIR}/cmake/AfsWxWidgets.cmake)
        target_compile_definitions(${target} PUBLIC _WXWIDGETS)
        target_link_libraries(${target} PUBLIC afs_wxwidgets)
        # Remaining upward dependency: SyncFile.cpp drives the copy progress
        # through AFS_Desktop's ProgressDialog (registered at SyncControler). It
        # has to go behind an interface owned by the core before the core can be
        # published on its own; until then the header is reachable from here.
        target_include_directories(${target} PRIVATE ${AFS_CORE_DIR}/../AFS_Desktop/include)
    endif()
endfunction()
