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
#ifdef _UNIX
#include "Config.h"
#include "FileUtil.h"
#include "MyDaemon.h"
#include "ScopedConfigHome.h"
#include "ScopedLoggingSetup.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Translation.h"
#include "Util.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <thread>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;

constexpr auto PASS_TIMEOUT = std::chrono::seconds(20);  // [DaemonTests.md#PASS_TIMEOUT]

bool wait_until(const std::function<bool()>& reached)
{
    const auto deadline = std::chrono::steady_clock::now() + PASS_TIMEOUT;
    while (!reached() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return reached();
}

class DaemonTest : public ::testing::Test {  // [DaemonTests.md#DaemonTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_daemon_" + std::to_string(TestPaths::process_id()));
    const tu::ScopedConfigHome config_home_{root_ / "config"};
    const tu::ScopedLoggingSetup logging_setup_;
    const std::wstring previous_pathname_ = Config::instance().pathname();

    void SetUp() override
    {
        fs::remove_all(root_);
        for (const char* name : {"eins", "zwei"}) {
            fs::create_directories(source(name));
            fs::create_directories(target(name));
        }
    }

    void TearDown() override
    {
        Synchronize::destroy();
        Config::instance().reset();
        Config::instance().pathname(previous_pathname_);
        translation::set_language("");
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    fs::path source(const char* name) const { return root_ / name / "quelle"; }
    fs::path target(const char* name) const { return root_ / name / "ziel"; }

    std::string configuration_text(std::initializer_list<const char*> names) const  // [DaemonTests.md#configuration_text]
    {
        Config& config = Config::instance();
        config.reset();
        for (const char* name : names) {
            SyncItem si(util::string_to_wstring(name), SyncItemType::Dir, source(name).wstring(), target(name).wstring(), L"");
            si.sync_cycle(0);
            config.add(si);
        }
        const fs::path draft = root_ / "entwurf.json";
        EXPECT_TRUE(config.write(draft.wstring()));
        config.reset();
        return tu::read_text_file(draft);
    }

    static void save_like_the_gui(const std::string& text)
    {
        tu::write_text_file(Config::get_root_pathname(), text);
    }
};

} // namespace

TEST_F(DaemonTest, DaemonSyncsTheItemsOfTheGuiConfiguration)  // [DaemonTests.md#DaemonSyncsTheItemsOfTheGuiConfiguration]
{
    tu::write_text_file(source("eins") / "a.txt", "vom Dienst kopiert");
    save_like_the_gui(configuration_text({"eins"}));
    MyDaemon daemon;

    daemon.start();

    EXPECT_TRUE(wait_until([&] { return fs::exists(target("eins") / "a.txt"); })) << "der Dienst hat den Eintrag der Konfiguration nicht synchronisiert";
    daemon.stop();
    EXPECT_FALSE(Synchronize::has_instance()) << "nach stop() darf kein Durchlauf mehr laufen";
    tu::write_text_file(source("eins") / "nach_dem_stopp.txt", "bleibt liegen");
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));  // [DaemonTests.md#stopped-means-stopped]
    EXPECT_FALSE(fs::exists(target("eins") / "nach_dem_stopp.txt"));
}

TEST_F(DaemonTest, DaemonReloadsAChangedConfiguration)  // [DaemonTests.md#DaemonReloadsAChangedConfiguration]
{
    tu::write_text_file(source("eins") / "a.txt", "erster Eintrag");
    tu::write_text_file(source("zwei") / "b.txt", "zweiter Eintrag");
    const std::string only_the_second = configuration_text({"zwei"});
    save_like_the_gui(configuration_text({"eins"}));
    MyDaemon daemon;
    daemon.start();
    ASSERT_TRUE(wait_until([&] { return fs::exists(target("eins") / "a.txt"); }));
    EXPECT_FALSE(fs::exists(target("zwei") / "b.txt")) << "der zweite Eintrag steht noch nicht in der Konfiguration";

    save_like_the_gui(only_the_second);

    EXPECT_TRUE(wait_until([&] { return fs::exists(target("zwei") / "b.txt"); })) << "der neue Eintrag wurde nach der Aenderung nicht uebernommen";
    tu::write_text_file(source("eins") / "entfernt.txt", "Eintrag gibt es nicht mehr");
    tu::write_text_file(source("zwei") / "c.txt", "beweist einen weiteren Durchlauf");
    ASSERT_TRUE(wait_until([&] { return fs::exists(target("zwei") / "c.txt"); }));
    EXPECT_FALSE(fs::exists(target("eins") / "entfernt.txt")) << "ein entfernter Eintrag darf nicht weiter synchronisiert werden";
    daemon.stop();
}
#endif
