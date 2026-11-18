/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include <SymlinkUtil.h>
#include <FilesystemUtil.h>
#include "TestPaths.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace AFS_Core;

namespace {

namespace fs = std::filesystem;

class SymlinkUtilTest : public ::testing::Test {  // [SymlinkUtilTests.md#SymlinkUtilTest]
protected:
    const fs::path root_ = fs::temp_directory_path() / ("autofilesync_symlink_util_" + std::to_string(TestPaths::process_id()));

    void SetUp() override
    {
        fs::remove_all(root_);
        fs::create_directories(root_);
        std::ofstream(root_ / "da.txt") << "da";
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
};

} // namespace

TEST_F(SymlinkUtilTest, DanglingLinkIsALinkWhoseTargetIsMissingOrRunsInACircle)  // [SymlinkUtilTests.md#DanglingLinkIsALinkWhoseTargetIsMissingOrRunsInACircle]
{
    fs::create_symlink(root_ / "da.txt", root_ / "gut");
    fs::create_symlink(root_ / "weg.txt", root_ / "tot");
    fs::create_symlink("kreis", root_ / "kreis");

    EXPECT_TRUE(symlink_util::dangling_link(root_ / "tot"));
    EXPECT_TRUE(symlink_util::dangling_link(root_ / "kreis"));
    EXPECT_FALSE(symlink_util::dangling_link(root_ / "gut"));
    EXPECT_FALSE(symlink_util::dangling_link(root_ / "da.txt")) << "a file is no link";
    EXPECT_FALSE(symlink_util::dangling_link(root_ / "weg.txt")) << "a missing file is no link either";
}

TEST_F(SymlinkUtilTest, LinkInfoTellsTheTextAndWhatTheLinkLeadsTo)  // [SymlinkUtilTests.md#LinkInfoTellsTheTextAndWhatTheLinkLeadsTo]
{
    fs::create_directories(root_ / "ordner");
    fs::create_symlink("da.txt", root_ / "datei");
    fs::create_directory_symlink(root_ / "ordner", root_ / "verzeichnis");
    fs::create_symlink("weg.txt", root_ / "tot");

    const symlink_util::LinkInfo file = symlink_util::link_info(root_ / "datei");
    const symlink_util::LinkInfo directory = symlink_util::link_info(root_ / "verzeichnis");
    const symlink_util::LinkInfo dead = symlink_util::link_info(root_ / "tot");

    EXPECT_TRUE(file.is_link && file.resolves && !file.to_directory);
    EXPECT_EQ(file.text, fs::path("da.txt")) << "the text as it is stored, not what it resolves to";
    EXPECT_TRUE(directory.is_link && directory.resolves && directory.to_directory);
    EXPECT_EQ(directory.text, root_ / "ordner");
    EXPECT_TRUE(dead.is_link && !dead.resolves && !dead.to_directory);
    EXPECT_EQ(dead.text, fs::path("weg.txt"));
    EXPECT_FALSE(symlink_util::link_info(root_ / "da.txt").is_link);
    EXPECT_FALSE(symlink_util::link_info(root_ / "weg.txt").is_link);
}

#ifndef _WIN32
TEST_F(SymlinkUtilTest, LinkInfoSaysWhenAnEntryCannotBeAsked)  // [SymlinkUtilTests.md#LinkInfoSaysWhenAnEntryCannotBeAsked]
{
    if (geteuid() == 0)
        GTEST_SKIP() << "root darf jedes Verzeichnis durchsuchen";
    fs::create_directories(root_ / "zu");
    fs::create_symlink("da.txt", root_ / "zu" / "link");
    fs::permissions(root_ / "zu", fs::perms::owner_read);

    const symlink_util::LinkInfo unasked = symlink_util::link_info(root_ / "zu" / "link");
    fs::permissions(root_ / "zu", fs::perms::owner_all);
    const symlink_util::LinkInfo asked = symlink_util::link_info(root_ / "zu" / "link");
    const symlink_util::LinkInfo missing = symlink_util::link_info(root_ / "zu" / "fehlt");

    EXPECT_EQ(unasked.refusal, fsu::FileInfoResult::PermissionDenied);
    EXPECT_FALSE(unasked.is_link) << "ohne Antwort ist nichts ueber den Eintrag bekannt";
    EXPECT_EQ(asked.refusal, fsu::FileInfoResult::OK);
    EXPECT_TRUE(asked.is_link);
    EXPECT_EQ(missing.refusal, fsu::FileInfoResult::OK) << "ein fehlender Eintrag ist eine Antwort";
    EXPECT_FALSE(missing.is_link);
}
#endif

TEST(SymlinkUtilTests, PortableLinkTextKeepsRelativeTextAndMakesAbsoluteTextInsideTheRootRelative)  // [SymlinkUtilTests.md#portable_link_text]
{
    const fs::path root = "/daten/quelle";

    EXPECT_EQ(symlink_util::portable_link_text(root / "aktuell", "2026", root), fs::path("2026"));
    EXPECT_EQ(symlink_util::portable_link_text(root / "a" / "aktuell", "../b/x.txt", root), fs::path("../b/x.txt")) << "relative text is never touched";
    EXPECT_EQ(symlink_util::portable_link_text(root / "aktuell", root / "2026", root), fs::path("2026"));
    EXPECT_EQ(symlink_util::portable_link_text(root / "a" / "b" / "link", root / "c" / "x.txt", root), fs::path("../../c/x.txt"));
    EXPECT_EQ(symlink_util::portable_link_text(root / "selbst", root, root), fs::path(".")) << "a link to the root itself";
    EXPECT_EQ(symlink_util::portable_link_text(root / "raus", "/etc/hosts", root), fs::path("/etc/hosts")) << "absolute text outside the root stays";
    EXPECT_EQ(symlink_util::portable_link_text(root / "nachbar", "/daten/quelle2/x", root), fs::path("/daten/quelle2/x")) << "a name that merely begins like the root is outside";
    EXPECT_EQ(symlink_util::portable_link_text(root / "umweg", root / "a" / ".." / ".." / "fremd", root), root / "a" / ".." / ".." / "fremd") << "dots that lead out of the root";
}

TEST(SymlinkUtilTests, LeavesRootTellsWhetherALinkPointsOutOfTheTree)  // [SymlinkUtilTests.md#leaves_root]
{
    const fs::path root = "/daten/quelle";

    EXPECT_FALSE(symlink_util::leaves_root(root / "aktuell", "2026", root));
    EXPECT_FALSE(symlink_util::leaves_root(root / "a" / "link", "../b/x.txt", root));
    EXPECT_FALSE(symlink_util::leaves_root(root / "link", root / "a", root));
    EXPECT_TRUE(symlink_util::leaves_root(root / "link", "../fremd", root));
    EXPECT_TRUE(symlink_util::leaves_root(root / "a" / "link", "../../fremd/x", root));
    EXPECT_TRUE(symlink_util::leaves_root(root / "link", "/etc/hosts", root));
    EXPECT_TRUE(symlink_util::leaves_root(root / "link", "/daten/quelle2", root));
}

#ifdef __linux__
TEST_F(SymlinkUtilTest, SystemLinkIsOneThatLeadsIntoTheSystemsOwnTreesOrToADevice)  // [SymlinkUtilTests.md#system_link]
{
    fs::create_symlink("/proc/version", root_ / "proc");
    fs::create_symlink("/dev/null", root_ / "geraet");
    fs::create_symlink("/sys/kernel", root_ / "sys");
    fs::create_symlink("da.txt", root_ / "eigen");
    fs::create_symlink("/run/media/niemand/stick/x.txt", root_ / "stick");
    fs::create_symlink("weg.txt", root_ / "tot");

    EXPECT_TRUE(symlink_util::system_link(root_ / "proc"));
    EXPECT_TRUE(symlink_util::system_link(root_ / "geraet"));
    EXPECT_TRUE(symlink_util::system_link(root_ / "sys"));
    EXPECT_FALSE(symlink_util::system_link(root_ / "eigen"));
    EXPECT_FALSE(symlink_util::system_link(root_ / "stick")) << "removable drives are mounted below /run/media and are the user's";
    EXPECT_FALSE(symlink_util::system_link(root_ / "tot"));
    EXPECT_FALSE(symlink_util::system_link(root_ / "da.txt")) << "a file is no link";
}
#endif

TEST_F(SymlinkUtilTest, CreateLinkRefusalCreatesTheLinkOrSaysWhyNot)  // [SymlinkUtilTests.md#create_link_refusal]
{
    fs::create_directories(root_ / "ordner");

    EXPECT_EQ(symlink_util::create_link_refusal("da.txt", root_ / "datei", false), fsu::FileInfoResult::OK);
    EXPECT_EQ(symlink_util::create_link_refusal("ordner", root_ / "verzeichnis", true), fsu::FileInfoResult::OK);
    EXPECT_EQ(symlink_util::create_link_refusal("weg.txt", root_ / "tot", false), fsu::FileInfoResult::OK) << "a link may point to nothing";

    EXPECT_EQ(fs::read_symlink(root_ / "datei"), fs::path("da.txt"));
    EXPECT_TRUE(fs::is_directory(root_ / "verzeichnis"));
    EXPECT_TRUE(fs::is_symlink(root_ / "tot"));
    EXPECT_EQ(symlink_util::create_link_refusal("da.txt", root_ / "datei", false), fsu::FileInfoResult::ErrorWriteFileInfo) << "a name that is taken";
    EXPECT_EQ(symlink_util::create_link_refusal("da.txt", root_ / "fehlt" / "datei", false), fsu::FileInfoResult::FileNotExist);
}

TEST_F(SymlinkUtilTest, LinkErrorForTestRefusesWithoutCreatingAnything)  // [SymlinkUtilTests.md#create_link_refusal]
{
    symlink_util::set_link_error_for_test(std::make_error_code(std::errc::operation_not_permitted));
    const fsu::FileInfoResult unsupported = symlink_util::create_link_refusal("da.txt", root_ / "datei", false);
    symlink_util::set_link_error_for_test(std::make_error_code(std::errc::io_error));
    const fsu::FileInfoResult failed = symlink_util::create_link_refusal("da.txt", root_ / "datei", false);
    symlink_util::set_link_error_for_test(std::nullopt);

    EXPECT_EQ(unsupported, fsu::FileInfoResult::LinksNotSupported) << "what a FAT drive answers";
    EXPECT_EQ(failed, fsu::FileInfoResult::ErrorWriteFileInfo);
    EXPECT_FALSE(fs::is_symlink(root_ / "datei"));
    EXPECT_EQ(symlink_util::create_link_refusal("da.txt", root_ / "datei", false), fsu::FileInfoResult::OK) << "the test value is gone again";
}

class LinkTrailTest : public SymlinkUtilTest {  // [SymlinkUtilTests.md#LinkTrailTest]
protected:
    const fs::path source_ = root_ / "quelle";
    const fs::path target_ = root_ / "ziel";

    void SetUp() override
    {
        SymlinkUtilTest::SetUp();
        fs::create_directories(source_ / "a" / "b");
        fs::create_directories(target_ / "t");
        fs::create_directories(root_ / "fremd" / "tief");
    }
};

TEST_F(LinkTrailTest, LinkOutOfTheTreeMayBeFollowed)  // [SymlinkUtilTests.md#LinkTrail]
{
    fs::create_directory_symlink(root_ / "fremd", source_ / "raus");
    const symlink_util::LinkTrail trail(source_, target_);

    EXPECT_EQ(trail.refusal(source_ / "raus"), symlink_util::FollowRefusal::None);
}

TEST_F(LinkTrailTest, LinkBackToADirectoryOnTheWayIsALoop)  // [SymlinkUtilTests.md#LinkTrail]
{
    fs::create_directory_symlink(source_, source_ / "a" / "b" / "hoch");
    fs::create_directory_symlink(".", source_ / "a" / "selbst");
    fs::create_directory_symlink(root_, source_ / "a" / "drueber");
    symlink_util::LinkTrail trail(source_, target_);
    trail.enter_directory("a");

    EXPECT_EQ(trail.refusal(source_ / "a" / "selbst"), symlink_util::FollowRefusal::Loop);
    EXPECT_EQ(trail.refusal(source_ / "a" / "drueber"), symlink_util::FollowRefusal::Loop) << "an ancestor of the root contains the root";
    trail.enter_directory("b");
    EXPECT_EQ(trail.refusal(source_ / "a" / "b" / "hoch"), symlink_util::FollowRefusal::Loop);
}

TEST_F(LinkTrailTest, SiblingReachedTwiceIsNoLoop)  // [SymlinkUtilTests.md#LinkTrail]
{
    fs::create_directories(source_ / "c");
    fs::create_directory_symlink(source_ / "c", source_ / "a" / "nachbar");
    symlink_util::LinkTrail trail(source_, target_);
    trail.enter_directory("a");

    EXPECT_EQ(trail.refusal(source_ / "a" / "nachbar"), symlink_util::FollowRefusal::None) << "c is not on the way down to a";
    trail.leave();
    trail.enter_directory("c");
    EXPECT_EQ(trail.refusal(source_ / "a" / "nachbar"), symlink_util::FollowRefusal::Loop) << "now it is";
}

TEST_F(LinkTrailTest, LoopThroughAFollowedLinkIsFound)  // [SymlinkUtilTests.md#LinkTrail]
{
    fs::create_directory_symlink(root_ / "fremd", source_ / "raus");
    fs::create_directory_symlink(root_ / "fremd", root_ / "fremd" / "tief" / "zurueck");
    symlink_util::LinkTrail trail(source_, target_);
    trail.enter_link(source_ / "raus");
    trail.enter_directory("tief");

    EXPECT_EQ(trail.refusal(source_ / "raus" / "tief" / "zurueck"), symlink_util::FollowRefusal::Loop);
}

TEST_F(LinkTrailTest, LinkIntoOrAboveTheTargetIsRefused)  // [SymlinkUtilTests.md#LinkTrail]
{
    fs::create_directory_symlink(target_, source_ / "ziel");
    fs::create_directory_symlink(target_ / "t", source_ / "imziel");
    const symlink_util::LinkTrail trail(source_, target_);

    EXPECT_EQ(trail.refusal(source_ / "ziel"), symlink_util::FollowRefusal::IntoTarget);
    EXPECT_EQ(trail.refusal(source_ / "imziel"), symlink_util::FollowRefusal::IntoTarget) << "what is copied would be read again";
}

TEST_F(LinkTrailTest, DanglingLinkAndLinkToAFileAreNotFollowed)  // [SymlinkUtilTests.md#LinkTrail]
{
    fs::create_symlink(root_ / "weg", source_ / "tot");
    fs::create_symlink(root_ / "da.txt", source_ / "datei");
    const symlink_util::LinkTrail trail(source_, target_);

    EXPECT_EQ(trail.refusal(source_ / "tot"), symlink_util::FollowRefusal::Unresolved);
    EXPECT_EQ(trail.refusal(source_ / "datei"), symlink_util::FollowRefusal::Unresolved) << "only directories are entered";
}

TEST_F(LinkTrailTest, ChainOfLinksEndsAtTheLimit)  // [SymlinkUtilTests.md#LinkTrail]
{
    symlink_util::LinkTrail trail(source_, target_);
    fs::path here = source_;
    for (int i = 0; i < symlink_util::MAX_FOLLOWED_LINKS; ++i) {
        const fs::path elsewhere = root_ / ("kette" + std::to_string(i));
        fs::create_directories(elsewhere);
        fs::create_directory_symlink(elsewhere, here / "weiter");
        ASSERT_EQ(trail.refusal(here / "weiter"), symlink_util::FollowRefusal::None) << i;
        trail.enter_link(here / "weiter");
        here = elsewhere;
    }
    fs::create_directory_symlink(root_ / "fremd", here / "weiter");

    EXPECT_EQ(trail.refusal(here / "weiter"), symlink_util::FollowRefusal::TooManyLinks);
    trail.leave();
    EXPECT_EQ(trail.refusal(here / "weiter"), symlink_util::FollowRefusal::None) << "leaving a link gives its place back";
}

TEST(SymlinkUtilTests, TrailOfResolvedPathsJudgesWithoutTheFileSystem)  // [SymlinkUtilTests.md#LinkTrail-resolved]
{
    symlink_util::LinkTrail trail = symlink_util::LinkTrail::of_resolved("/srv/daten", fs::path("/srv/sicherung"));
    trail.enter_directory("a");

    EXPECT_EQ(trail.refusal_of("/srv/anderes"), symlink_util::FollowRefusal::None);
    EXPECT_EQ(trail.refusal_of("/srv/daten"), symlink_util::FollowRefusal::Loop);
    EXPECT_EQ(trail.refusal_of("/srv/daten/a/"), symlink_util::FollowRefusal::Loop) << "a trailing separator changes nothing";
    EXPECT_EQ(trail.refusal_of("/srv"), symlink_util::FollowRefusal::Loop) << "above the root";
    EXPECT_EQ(trail.refusal_of("/srv/sicherung/alt"), symlink_util::FollowRefusal::IntoTarget);
    trail.enter_resolved("/srv/anderes");
    EXPECT_EQ(trail.refusal_of("/srv/anderes"), symlink_util::FollowRefusal::Loop);
    trail.leave();
    EXPECT_EQ(trail.refusal_of("/srv/anderes"), symlink_util::FollowRefusal::None);
}

TEST_F(SymlinkUtilTest, JunctionIsAWindowsThingAndNeverSeenElsewhere)  // [SymlinkUtilTests.md#junction]
{
    fs::create_directories(root_ / "ordner");
    fs::create_directory_symlink("ordner", root_ / "verzeichnis");

    EXPECT_FALSE(symlink_util::junction(root_ / "ordner"));
    EXPECT_FALSE(symlink_util::junction(root_ / "verzeichnis")) << "a symbolic link is no junction";
    EXPECT_FALSE(symlink_util::junction(root_ / "weg"));
}
#endif
