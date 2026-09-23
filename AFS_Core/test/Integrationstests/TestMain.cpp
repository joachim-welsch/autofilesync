/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include <Logging.h>
#include "TestPaths.h"
#include "FileUtil.h"
#include "SshTest.h"
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char* PRIVATE_TEMP_DIR_VARIABLE = "AFS_TEST_TEMP_DIR";  // [TestMain.md#private-temp-dir]

using Snapshot = std::set<std::string>;

Snapshot snapshot_of(const fs::path& dir)
{
    Snapshot entries;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        entries.insert(it->path().string());
    return entries;
}

std::string difference(const Snapshot& from, const Snapshot& to, const std::string& label)
{
    std::string text;
    for (const std::string& entry: to)
        if (!from.contains(entry))
            text += "\n  " + label + " " + entry;
    return text;
}

std::vector<std::string> leaks;  // [TestMain.md#leaks-fail-in-the-environment]

class LeftoverListener : public ::testing::EmptyTestEventListener {  // [TestMain.md#LeftoverListener]
    fs::path watched_;
    Snapshot before_;
public:
    explicit LeftoverListener(fs::path watched): watched_(std::move(watched)) {}
    void OnTestStart(const ::testing::TestInfo&) override
    {
        before_ = snapshot_of(watched_);
    }
    void OnTestEnd(const ::testing::TestInfo& test) override
    {
        const Snapshot after = snapshot_of(watched_);
        const std::string changes = difference(before_, after, "left behind:") + difference(after, before_, "removed:");
        if (!changes.empty())
            leaks.push_back(std::string(test.test_suite_name()) + "." + test.name() + changes);
    }
};

#ifndef _WIN32
void remove_entries_of_the_ssh_user(const fs::path& dir)  // [TestMain.md#entries-of-the-ssh-user]
{
    std::vector<fs::path> foreign;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        struct stat entry{};
        if (lstat(it->path().c_str(), &entry) != 0 || entry.st_uid == getuid())
            continue;
        foreign.push_back(it->path());
        it.disable_recursion_pending();
    }
    for (const fs::path& path: foreign)
        SSHTest::remove_remote_tree(path.string());
}
#endif

void remove_arbiter_locks()  // [TestMain.md#arbiter-locks]
{
    std::error_code ec;
    for (const fs::directory_entry& entry: fs::directory_iterator(fs::temp_directory_path(), ec))
        if (entry.path().extension() == ".lock" && entry.path().filename().string().starts_with("autofilesync-"))
            fs::remove(entry.path(), ec);
}

class ScratchDirCleaner : public ::testing::EmptyTestEventListener {  // [TestMain.md#ScratchDirCleaner]
public:
    void OnTestEnd(const ::testing::TestInfo&) override
    {
#ifndef _WIN32
        if (std::getenv(PRIVATE_TEMP_DIR_VARIABLE) != nullptr)
            remove_entries_of_the_ssh_user(fs::temp_directory_path());
#endif
        std::error_code ec;
        fs::remove_all(TestPaths::scratch_dir(), ec);
        fs::remove_all(fs::path(tu::test_source_dir()).parent_path(), ec);
        remove_arbiter_locks();
    }
};

class ScratchLogEnvironment : public ::testing::Environment {  // [TestMain.md#ScratchLogEnvironment]
    fs::path log_path_;
public:
    explicit ScratchLogEnvironment(fs::path log_path): log_path_(std::move(log_path)) {}
    void SetUp() override
    {
        AFS_Core::Logging::instance().set_log_file_path_name(log_path_.wstring());
    }
    void TearDown() override
    {
        AFS_Core::Logging::shutdown();
        fs::remove(log_path_);
        for (const std::string& leak: leaks)
            ADD_FAILURE() << "test changed the file system: " << leak;
    }
};

int run_tests(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    const fs::path temp_dir = fs::temp_directory_path();
    ::testing::AddGlobalTestEnvironment(new ScratchLogEnvironment(temp_dir / "autofilesync_integrationtests.log"));
    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
    if (std::getenv(PRIVATE_TEMP_DIR_VARIABLE) != nullptr)
        listeners.Append(new LeftoverListener(temp_dir));
    listeners.Append(new ScratchDirCleaner);  // [TestMain.md#listener-order]
    return RUN_ALL_TESTS();
}

#ifndef _WIN32
void use_private_home(const fs::path& home)  // [TestMain.md#private-home]
{
    const char* real_home = std::getenv("HOME");
    std::error_code ec;
    fs::create_directories(home / ".ssh");
    if (real_home != nullptr)
        fs::copy_file(fs::path(real_home) / ".ssh" / "known_hosts", home / ".ssh" / "known_hosts", ec);
    setenv("HOME", home.c_str(), 1);
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_CACHE_HOME");
}

int run_in_private_temp_dir(char* argv[])  // [TestMain.md#private-temp-dir]
{
    const fs::path temp_dir = fs::temp_directory_path() / ("autofilesync_integrationtests_" + std::to_string(getpid()));
    fs::create_directories(temp_dir);
    fs::permissions(temp_dir, fs::perms::all);  // [TestMain.md#ssh-user-writes-here]
    use_private_home(temp_dir / "home");
    setenv("TMPDIR", temp_dir.c_str(), 1);
    setenv(PRIVATE_TEMP_DIR_VARIABLE, temp_dir.c_str(), 1);
    const pid_t child = fork();
    if (child == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
#endif

} // namespace

int main(int argc, char* argv[])
{
#ifndef _WIN32
    if (std::getenv(PRIVATE_TEMP_DIR_VARIABLE) == nullptr)
        return run_in_private_temp_dir(argv);
#endif
    return run_tests(argc, argv);
}
