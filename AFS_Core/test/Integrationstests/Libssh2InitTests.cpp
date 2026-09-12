#include "pch.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include "Libssh2Init.h"
#include <libssh2.h>
using namespace AFS_Core;
#endif

namespace fs = std::filesystem;

namespace {

std::string read_file(const fs::path& path)
{
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST(Libssh2LifecycleTest, OnlyLibssh2InitCppCallsLibssh2InitOrExit)  // [Libssh2InitTests.md#OnlyLibssh2InitCppCallsLibssh2InitOrExit]
{
    const fs::path lib_dir = LIB_SOURCE_DIR;
    for (const auto& entry : fs::directory_iterator(lib_dir)) {
        if (!entry.is_regular_file())
            continue;
        const fs::path& path = entry.path();
        if (path.extension() != ".cpp" && path.extension() != ".h")
            continue;
        if (path.filename() == "Libssh2Init.cpp" || path.filename() == "Libssh2Init.h")
            continue;

        const std::string content = read_file(path);
        EXPECT_EQ(content.find("libssh2_init("), std::string::npos)
            << path.filename() << " calls libssh2_init() directly; use ensure_libssh2_initialized() instead.";
        EXPECT_EQ(content.find("libssh2_exit("), std::string::npos)
            << path.filename() << " calls libssh2_exit() directly; the global guard owns process-wide cleanup.";
    }
}

#ifdef _WIN32

TEST(Libssh2LifecycleTest, EnsureLibssh2InitializedIsIdempotent)  // [Libssh2InitTests.md#EnsureLibssh2InitializedIsIdempotent]
{
    for (int i = 0; i < 50; ++i)
        ensure_libssh2_initialized();

    LIBSSH2_SESSION* session = libssh2_session_init();
    EXPECT_NE(session, nullptr);
    if (session)
        libssh2_session_free(session);
}

TEST(Libssh2LifecycleTest, EnsureLibssh2InitializedIsThreadSafe)  // [Libssh2InitTests.md#EnsureLibssh2InitializedIsThreadSafe]
{
    std::vector<std::thread> threads;
    for (int i = 0; i < 16; ++i)
        threads.emplace_back(ensure_libssh2_initialized);
    for (auto& t : threads)
        t.join();

    LIBSSH2_SESSION* session = libssh2_session_init();
    EXPECT_NE(session, nullptr);
    if (session)
        libssh2_session_free(session);
}

#endif
