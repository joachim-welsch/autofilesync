/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#include "pch.h"
#include "ModelLocation.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>
#include <vector>
#ifdef _WIN32
    #include <process.h>   // _getpid
#else
    #include <unistd.h>    // getpid
#endif

using namespace AFS_Core;
namespace fs = std::filesystem;

namespace {

const char* const REDIRECTED_VARIABLES[] = {  // [ModelLocationTests.md#REDIRECTED_VARIABLES]
    model_location::PATH_VARIABLE,
#ifdef _WIN32
    "LOCALAPPDATA", "USERPROFILE"
#else
    "HOME", "XDG_DATA_HOME"
#endif
};

std::optional<std::string> get_env_var(const char* name)
{
    const char* value = getenv(name);
    return value ? std::optional<std::string>(value) : std::nullopt;
}

void set_env_var(const char* name, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void unset_env_var(const char* name)  // an empty value is how the CRT deletes a variable; POSIX has its own call
{
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

int process_id()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

class ModelLocationTest : public ::testing::Test {  // [ModelLocationTests.md#ModelLocationTest]
protected:
    fs::path test_home_;
    std::vector<std::pair<const char*, std::optional<std::string>>> saved_variables_;

    fs::path data_dir() const  // where the platform keeps per-user data once the environment is redirected
    {
#ifdef _WIN32
        return test_home_ / "Local";
#elif defined(_MACOS)
        return test_home_ / "Library" / "Application Support";
#else
        return test_home_ / ".local" / "share";
#endif
    }

    fs::path models_dir() const
    {
        return data_dir() / "autofilesync" / "models";
    }

    fs::path put_file(const std::string& name) const
    {
        const fs::path path = models_dir() / name;
        fs::create_directories(path.parent_path());
        std::ofstream(path) << "GGUF";
        return path;
    }

    void SetUp() override
    {
        for (const char* name : REDIRECTED_VARIABLES)
            saved_variables_.push_back({name, get_env_var(name)});
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        test_home_ = fs::temp_directory_path() / ("autofilesync_modeltest_" + test_name + "_" + std::to_string(process_id()));
        fs::remove_all(test_home_);
        fs::create_directories(test_home_);
        unset_env_var(model_location::PATH_VARIABLE);
#ifdef _WIN32
        set_env_var("LOCALAPPDATA", data_dir().string());
        set_env_var("USERPROFILE", test_home_.string());
#else
        set_env_var("HOME", test_home_.string());
        unset_env_var("XDG_DATA_HOME");
#endif
    }

    void TearDown() override
    {
        for (const auto& [name, value] : saved_variables_) {
            if (value)
                set_env_var(name, *value);
            else
                unset_env_var(name);
        }
        fs::remove_all(test_home_);
    }
};

}

TEST_F(ModelLocationTest, DirectoryLiesInUserData)
{
    EXPECT_EQ(model_location::directory().string(), models_dir().string());
}

TEST_F(ModelLocationTest, NothingFoundWithoutModelDirectory)
{
    EXPECT_TRUE(model_location::find().empty());
}

TEST_F(ModelLocationTest, FindsModelInModelDirectory)
{
    const fs::path model = put_file("qwen.gguf");
    EXPECT_EQ(model_location::find().string(), model.string());
}

TEST_F(ModelLocationTest, IgnoresPartialDownloadsAndOtherFiles)  // [ModelLocationTests.md#IgnoresPartialDownloadsAndOtherFiles]
{
    put_file("qwen.gguf.part");
    put_file("readme.txt");
    fs::create_directories(models_dir() / "folder.gguf");
    EXPECT_TRUE(model_location::find().empty());
}

TEST_F(ModelLocationTest, PicksFirstModelByName)
{
    put_file("b.gguf");
    const fs::path first = put_file("a.gguf");
    put_file("c.gguf");
    EXPECT_EQ(model_location::find().string(), first.string());
}

TEST_F(ModelLocationTest, VariableTakesPrecedenceOverDirectory)  // [ModelLocationTests.md#VariableTakesPrecedenceOverDirectory]
{
    put_file("a.gguf");
    const fs::path configured = test_home_ / "elsewhere.gguf";
    set_env_var(model_location::PATH_VARIABLE, configured.string());
    EXPECT_EQ(model_location::find().string(), configured.string());
}

#ifdef _WIN32
TEST_F(ModelLocationTest, DirectoryFallsBackToProfileWithoutLocalAppData)
{
    unset_env_var("LOCALAPPDATA");
    EXPECT_EQ(model_location::directory().string(), (test_home_ / "AppData" / "Local" / "autofilesync" / "models").string());
}
#elif !defined(_MACOS)
TEST_F(ModelLocationTest, DirectoryFollowsXdgDataHome)
{
    const fs::path xdg = test_home_ / "xdg";
    set_env_var("XDG_DATA_HOME", xdg.string());
    EXPECT_EQ(model_location::directory().string(), (xdg / "autofilesync" / "models").string());
}
#endif
