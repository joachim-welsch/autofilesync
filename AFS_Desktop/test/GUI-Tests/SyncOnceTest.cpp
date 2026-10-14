/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#include "GuiTest.h"
#include "EventPump.h"
#include "GuiTestSandbox.h"
#include "ModalDialogWatcher.h"
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "Translation.h"
#include <filesystem>
#include "id.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

wxString no_files_found()  // translated at the call, once the catalogue is bound
{
    return translation::translate("no_files_to_sync");
}
constexpr int PAUSE_MILLISECONDS = 1100;  // the item's sync cycle is 1 s, a run inside it is skipped

SyncItemInput dir_sync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

}

TEST_F(GuiTest, EinmalSynchronisierenKopiertDateiInsZiel)  // [SyncOnceTest.md#EinmalSynchronisierenKopiertDateiInsZiel]
{
    const SandboxTree tree(GuiTestSandbox::root() / "einmal", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-einmal"))) << "Die Synchronisation wurde nicht angelegt.";

    ASSERT_TRUE(sync_once(main_window())) << "Die Synchronisation ist nicht zu Ende gelaufen.";

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "Bericht") << "Die Datei wurde nicht ins Ziel kopiert.";
    EXPECT_EQ(tree.entries(), (std::vector<std::string>{"quelle", "quelle/bericht.txt", "ziel", "ziel/bericht.txt"}));
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce))) << "\"Einmal synchronisieren\" bleibt nach dem Lauf gesperrt.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::StartSync))) << "\"Start\" bleibt nach dem Lauf gesperrt.";
}

TEST_F(GuiTest, EinmalSynchronisierenVerzeichnisbaumKopiertAlleEbenen)
{
    const SandboxTree tree(GuiTestSandbox::root() / "baum",
        {{"quelle/oben.txt", "1"}, {"quelle/a/mitte.txt", "2"}, {"quelle/a/b/unten.txt", "3"}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(tree, "guitest-baum");
    input.type = SyncItemType::Tree;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("ziel/oben.txt"), "1");
    EXPECT_EQ(tree.read("ziel/a/mitte.txt"), "2");
    EXPECT_EQ(tree.read("ziel/a/b/unten.txt"), "3") << "Die unterste Ebene fehlt im Ziel.";
}

TEST_F(GuiTest, EinmalSynchronisierenVerzeichnisKopiertKeineUnterverzeichnisse)  // [SyncOnceTest.md#dir-versus-tree]
{
    const SandboxTree tree(GuiTestSandbox::root() / "flach", {{"quelle/oben.txt", "1"}, {"quelle/a/unten.txt", "2"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-flach")));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("ziel/oben.txt"), "1");
    EXPECT_FALSE(tree.exists("ziel/a")) << "Typ Verzeichnis hat ein Unterverzeichnis mitkopiert.";
}

TEST_F(GuiTest, EinmalSynchronisierenOhneAenderungKopiertNichts)  // [SyncOnceTest.md#nothing-to-do]
{
    const SandboxTree tree(GuiTestSandbox::root() / "unveraendert", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-unveraendert")));
    ASSERT_TRUE(sync_once(main_window()));
    ASSERT_TRUE(tree.exists("ziel/bericht.txt"));
    const auto copied_at = std::filesystem::last_write_time(tree.path("ziel/bericht.txt"));

    pump(PAUSE_MILLISECONDS);
    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(std::filesystem::last_write_time(tree.path("ziel/bericht.txt")), copied_at) << "Die unveränderte Datei wurde erneut kopiert.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answered_messages().empty());
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce)));
}

TEST_F(GuiTest, EinmalSynchronisierenInDerPauseMeldetNichtsZuTun)  // [SyncOnceTest.md#pause]
{
    const SandboxTree tree(GuiTestSandbox::root() / "pause", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(tree, "guitest-pause");
    input.options.sync_cycle_seconds = 60;  // long enough that the second run surely falls into the pause
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));

    ModalDialogWatcher::instance().answer(translation::translate("info"), wxID_OK);
    ASSERT_TRUE(sync_once(main_window()));

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Der Lauf in der Pause zeigte keinen Hinweis.";
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().front(), no_files_found());
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce))) << "\"Einmal synchronisieren\" bleibt nach dem Hinweis gesperrt.";
}

TEST_F(GuiTest, InaktiveSynchronisationWirdUebersprungen)
{
    const SandboxTree tree(GuiTestSandbox::root() / "inaktiv", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(tree, "guitest-inaktiv");
    input.options.enabled = false;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ModalDialogWatcher::instance().answer(translation::translate("info"), wxID_OK);
    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_FALSE(tree.exists("ziel/bericht.txt")) << "Eine inaktive Synchronisation hat kopiert.";
    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u);
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().front(), no_files_found());
}

TEST_F(GuiTest, StartOhneSynchronisationZeigtHinweis)  // [SyncOnceTest.md#start-without-sync-item]
{
    ASSERT_TRUE(Config::instance().syncitems().empty()) << "Die Sandbox enthält schon Synchronisationen.";
    ModalDialogWatcher::instance().answer(translation::translate("attention"), wxID_OK);

    activate_menu_item(main_window(), tul(id::StartSync));
    ASSERT_TRUE(wait_until([] { return !ModalDialogWatcher::instance().answered_messages().empty(); }))
        << "Der Hinweis auf die fehlende Synchronisation kam nicht.";
    activate_menu_item(main_window(), tul(id::StopSync));
    ASSERT_TRUE(wait_until_sync_finished(main_window()));

    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().front(), translation::translate("no_sync_items"));
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::StartSync)));
    EXPECT_FALSE(sync_menu_item_enabled(main_window(), tul(id::StopSync)));
}

}
