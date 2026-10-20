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
#include "id.h"
#include "utilwx.h"
#include "RecycleBin.h"
#include <algorithm>
#include <chrono>
#include <filesystem>

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace guitest {
namespace {

class EmptiedRecycleBin {  // [SyncOptionsTest.md#EmptiedRecycleBin]
public:
    EmptiedRecycleBin()
    {
        const fs::path bin = RecycleBin::instance().bin_path();
        inside_sandbox_ = std::mismatch(GuiTestSandbox::root().begin(), GuiTestSandbox::root().end(), bin.begin(), bin.end()).first == GuiTestSandbox::root().end();
        EXPECT_TRUE(inside_sandbox_) << "Der Papierkorb liegt außerhalb der Sandbox: " << bin;
    }
    ~EmptiedRecycleBin()
    {
        std::error_code ignored;
        if (inside_sandbox_)  // [SyncOptionsTest.md#EmptiedRecycleBin]
            fs::remove_all(RecycleBin::instance().bin_path(), ignored);
    }
    std::size_t items() const { return RecycleBin::instance().list_items().size(); }
private:
    bool inside_sandbox_ = false;
};

SyncItemInput dir_sync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

void same_write_time(const SandboxTree& tree, const char* first, const char* second)  // [SyncOptionsTest.md#same_write_time]
{
    fs::last_write_time(tree.path(second), fs::last_write_time(tree.path(first)));
}

void write_time_apart(const SandboxTree& tree, const char* newer, const char* older, std::chrono::seconds gap)  // [SyncOptionsTest.md#write_time_apart]
{
    fs::last_write_time(tree.path(newer), fs::last_write_time(tree.path(older)) + gap);
}

void let_the_pause_elapse()  // [SyncOptionsTest.md#let_the_pause_elapse]
{
    pump(1100);
}

void age_file(const SandboxTree& tree, const char* file)  // [SyncOptionsTest.md#GenerationenprinzipLegtBakDateiAn]
{
    fs::last_write_time(tree.path(file), fs::file_time_type::clock::now() - std::chrono::hours(1));
}

}

TEST_F(GuiTest, ZweiWegeSyncKopiertInBeideRichtungen)
{
    const SandboxTree tree(GuiTestSandbox::root() / "zweiwege", {{"quelle/hin.txt", "hin"}, {"ziel/zurueck.txt", "zurück"}});
    SyncItemInput input = dir_sync(tree, "guitest-zweiwege");
    input.options.two_way_sync = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("ziel/hin.txt"), "hin");
    EXPECT_EQ(tree.read("quelle/zurueck.txt"), "zurück") << "Die nur im Ziel vorhandene Datei kam nicht in die Quelle.";
}

TEST_F(GuiTest, QuelleNachKopierenLoeschenEntferntDieQuelldatei)  // [SyncOptionsTest.md#recycle-bin-tests]
{
    const EmptiedRecycleBin recycle_bin;
    const SandboxTree tree(GuiTestSandbox::root() / "verschieben", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(tree, "guitest-verschieben");
    input.options.delete_source_after = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "Bericht");
    EXPECT_FALSE(tree.exists("quelle/bericht.txt")) << "Die Quelldatei blieb nach dem Kopieren stehen.";
    EXPECT_EQ(recycle_bin.items(), 1u) << "Die Quelldatei ging nicht in den Papierkorb.";
}

TEST_F(GuiTest, GeloeschteDateienImZielLoeschenEntferntVerwaisteDatei)  // [SyncOptionsTest.md#recycle-bin-tests]
{
    const EmptiedRecycleBin recycle_bin;
    const SandboxTree tree(GuiTestSandbox::root() / "spiegel", {{"quelle/bleibt.txt", "1"}, {"ziel/verwaist.txt", "2"}});
    SyncItemInput input = dir_sync(tree, "guitest-spiegel");
    input.options.delete_in_target = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_TRUE(tree.exists("ziel/bleibt.txt"));
    EXPECT_FALSE(tree.exists("ziel/verwaist.txt")) << "Die verwaiste Zieldatei wurde nicht gelöscht.";
    EXPECT_EQ(recycle_bin.items(), 1u) << "Die Zieldatei ging nicht in den Papierkorb.";
}

TEST_F(GuiTest, OhneOptionBleibtVerwaisteDateiImZiel)
{
    const SandboxTree tree(GuiTestSandbox::root() / "keinspiegel", {{"quelle/bleibt.txt", "1"}, {"ziel/verwaist.txt", "2"}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-keinspiegel")));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_TRUE(tree.exists("ziel/verwaist.txt")) << "Ohne die Option wurde im Ziel gelöscht.";
}

TEST_F(GuiTest, GenerationenprinzipLegtBakDateiAn)  // [SyncOptionsTest.md#GenerationenprinzipLegtBakDateiAn]
{
    const SandboxTree tree(GuiTestSandbox::root() / "generationen", {{"quelle/bericht.txt", "alt"}, {"ziel/", ""}});
    age_file(tree, "quelle/bericht.txt");
    SyncItemInput input = dir_sync(tree, "guitest-generationen");
    input.options.father_son_principle = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));
    ASSERT_EQ(tree.read("ziel/bericht.txt"), "alt");

    pump(1100);  // [SyncOptionsTest.md#GenerationenprinzipLegtBakDateiAn]
    tree.write("quelle/bericht.txt", "neu");
    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "neu");
    EXPECT_EQ(tree.read("ziel/bericht.txt.bak.1"), "alt") << "Der alte Stand wurde nicht als .bak.1 aufgehoben.";
}

TEST_F(GuiTest, LeereDateienWerdenNurMitOptionKopiert)
{
    const SandboxTree tree(GuiTestSandbox::root() / "leer", {{"quelle/leer.txt", ""}, {"quelle/voll.txt", "x"}, {"ziel/", ""}});
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-leer")));
    ASSERT_TRUE(sync_once(main_window()));
    ASSERT_TRUE(tree.exists("ziel/voll.txt"));
    ASSERT_FALSE(tree.exists("ziel/leer.txt")) << "Die leere Datei wurde ohne die Option kopiert.";

    const SandboxTree with_option(GuiTestSandbox::root() / "leer-mit", {{"quelle/leer.txt", ""}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(with_option, "guitest-leer-mit");
    input.options.sync_empty_files = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_TRUE(with_option.exists("ziel/leer.txt")) << "Die leere Datei wurde trotz Option nicht kopiert.";
}

TEST_F(GuiTest, VergleichNachInhaltKopiertGeaenderteDateiGleicherGroesse)  // [SyncOptionsTest.md#same_write_time]
{
    const SandboxTree tree(GuiTestSandbox::root() / "inhalt", {{"quelle/bericht.txt", "AAAA"}, {"ziel/bericht.txt", "BBBB"}});
    same_write_time(tree, "quelle/bericht.txt", "ziel/bericht.txt");
    SyncItemInput input = dir_sync(tree, "guitest-inhalt");
    input.options.compare_date = false;
    input.options.compare_content = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "AAAA") << "Der Inhaltsunterschied wurde nicht erkannt.";
}

TEST_F(GuiTest, ZeitToleranzUebergehtKleineZeitunterschiede)  // [SyncOptionsTest.md#write_time_apart]
{
    const SandboxTree tree(GuiTestSandbox::root() / "toleranz", {{"quelle/bericht.txt", "AAAA"}, {"ziel/bericht.txt", "BBBB"}});
    write_time_apart(tree, "quelle/bericht.txt", "ziel/bericht.txt", std::chrono::seconds(1));
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-toleranz")));
    ASSERT_TRUE(sync_once(main_window()));
    ASSERT_EQ(tree.read("ziel/bericht.txt"), "BBBB") << "Eine Sekunde Unterschied wurde trotz Toleranz als Änderung gewertet.";

    const SandboxTree exact(GuiTestSandbox::root() / "toleranz-null", {{"quelle/bericht.txt", "AAAA"}, {"ziel/bericht.txt", "BBBB"}});
    write_time_apart(exact, "quelle/bericht.txt", "ziel/bericht.txt", std::chrono::seconds(1));
    SyncItemInput input = dir_sync(exact, "guitest-toleranz-null");
    input.options.time_tolerance_seconds = 0;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(exact.read("ziel/bericht.txt"), "AAAA") << "Mit Toleranz 0 wurde die neuere Quelle nicht kopiert.";
}

TEST_F(GuiTest, VergleichNachDatumUebersiehtGeaenderteDateiGleicherGroesse)
{
    const SandboxTree tree(GuiTestSandbox::root() / "datum", {{"quelle/bericht.txt", "AAAA"}, {"ziel/bericht.txt", "BBBB"}});
    same_write_time(tree, "quelle/bericht.txt", "ziel/bericht.txt");
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-datum")));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("ziel/bericht.txt"), "BBBB") << "Ohne Inhaltsvergleich wurde bei gleichem Datum kopiert.";
}

TEST_F(GuiTest, PlatzhalterBeschraenktAufPassendeDateien)
{
    const SandboxTree tree(GuiTestSandbox::root() / "platzhalter", {{"quelle/a.txt", "1"}, {"quelle/b.log", "2"}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(tree, "guitest-platzhalter");
    input.wildcard = "*.txt";
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_TRUE(tree.exists("ziel/a.txt"));
    EXPECT_FALSE(tree.exists("ziel/b.log")) << "Eine Datei außerhalb des Platzhalters wurde kopiert.";
}

TEST_F(GuiTest, AusschluesseHaltenDateienZurueck)
{
    const SandboxTree tree(GuiTestSandbox::root() / "ausschluss",
        {{"quelle/a.txt", "1"}, {"quelle/b.log", "2"}, {"quelle/geheim.txt", "3"}, {"ziel/", ""}});
    SyncItemInput input = dir_sync(tree, "guitest-ausschluss");
    input.exclusions = "*.log\ngeheim*";
    ASSERT_TRUE(add_sync_item(main_window(), input));
    const auto created = Config::instance().find_by_name(input.name.ToStdWstring());
    ASSERT_TRUE(created.has_value());
    ASSERT_EQ(created->exclusions().size(), 2u) << "Die Ausschlüsse kamen nicht zeilenweise in die Konfiguration.";

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_TRUE(tree.exists("ziel/a.txt"));
    EXPECT_FALSE(tree.exists("ziel/b.log")) << "Ein per Muster ausgeschlossener Typ wurde kopiert.";
    EXPECT_FALSE(tree.exists("ziel/geheim.txt")) << "Eine per Namensmuster ausgeschlossene Datei wurde kopiert.";
}


TEST_F(GuiTest, ZweiWegeSyncUebernimmtLoeschungBeimZweitenLauf)  // [SyncOptionsTest.md#two-way-state]
{
    const EmptiedRecycleBin recycle_bin;
    const SandboxTree tree(GuiTestSandbox::root() / "zweiwege-loeschen", {{"quelle/weg.txt", "wird gelöscht"}, {"quelle/bleibt.txt", "bleibt"}});
    fs::create_directories(tree.path("ziel"));  // [PreviewTest.md#existing-target]
    SyncItemInput input = dir_sync(tree, "guitest-zweiwege-loeschen");
    input.options.two_way_sync = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));
    ASSERT_TRUE(tree.exists("ziel/weg.txt"));
    fs::remove(tree.path("quelle/weg.txt"));
    let_the_pause_elapse();

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_FALSE(tree.exists("ziel/weg.txt")) << "Die in der Quelle gelöschte Datei kam aus dem Ziel zurück oder blieb dort.";
    EXPECT_FALSE(tree.exists("quelle/weg.txt")) << "Die gelöschte Datei wurde aus dem Ziel zurückkopiert.";
    EXPECT_EQ(tree.read("ziel/bleibt.txt"), "bleibt");
    EXPECT_EQ(recycle_bin.items(), 1u) << "Die Zieldatei ging nicht in den Papierkorb.";
}

TEST_F(GuiTest, UmbenennenErkennenBenenntImZielUm)  // [SyncOptionsTest.md#two-way-state]
{
    const EmptiedRecycleBin recycle_bin;
    const SandboxTree tree(GuiTestSandbox::root() / "umbenennen", {{"quelle/alt.txt", "umbenannt"}});
    fs::create_directories(tree.path("ziel"));  // [PreviewTest.md#existing-target]
    SyncItemInput input = dir_sync(tree, "guitest-umbenennen");
    input.options.delete_in_target = true;
    input.options.track_moves = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));
    ASSERT_TRUE(tree.exists("ziel/alt.txt"));
    fs::rename(tree.path("quelle/alt.txt"), tree.path("quelle/neu.txt"));
    let_the_pause_elapse();

    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("ziel/neu.txt"), "umbenannt");
    EXPECT_FALSE(tree.exists("ziel/alt.txt"));
    EXPECT_EQ(recycle_bin.items(), 0u) << "Die Datei wurde gelöscht und neu kopiert statt umbenannt.";
}

TEST_F(GuiTest, MassenloeschungWirdVerweigertUndAusDerVorschauAusgefuehrt)  // [SyncOptionsTest.md#mass-deletion]
{
    const EmptiedRecycleBin recycle_bin;
    FileContents files{{"ziel/", ""}};
    for (int i = 0; i < 12; ++i)
        files.emplace_back("quelle/datei" + std::to_string(i) + ".txt", "x");
    const SandboxTree tree(GuiTestSandbox::root() / "massenloeschung", files);
    SyncItemInput input = dir_sync(tree, "guitest-massenloeschung");
    input.options.two_way_sync = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));
    ASSERT_TRUE(tree.exists("ziel/datei11.txt"));
    fs::remove_all(tree.path("ziel"));
    fs::create_directories(tree.path("ziel"));
    let_the_pause_elapse();

    ModalDialogWatcher::instance().answer(translation::translate("synchronisation") + L": " + translation::translate("error"), wxID_OK);
    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_TRUE(tree.exists("quelle/datei0.txt")) << "Der Lauf hat die Quelle trotz Schwelle geleert.";
    EXPECT_EQ(recycle_bin.items(), 0u);
    EXPECT_TRUE(ModalDialogWatcher::instance().unexpected_dialogs().empty()) << "Ein unerwarteter Dialog erschien.";
    EXPECT_FALSE(ModalDialogWatcher::instance().has_pending_expectations()) << "Die Fehlerbox mit der Verweigerung fehlt.";
    let_the_pause_elapse();

    ModalDialogWatcher::instance().expect(translation::translate("preview_title"), [](wxDialog& dialog) { click_on(find(dialog, wxID_OK)); });
    activate_menu_item(main_window(), utilwx::tul(id::Preview));
    pump();
    ASSERT_TRUE(wait_until_sync_finished(main_window())) << "Der Lauf nach der Vorschau kam nicht zur Ruhe.";

    EXPECT_FALSE(tree.exists("quelle/datei0.txt")) << "\"Jetzt synchronisieren\" in der Vorschau führt die Löschungen nicht aus.";
    EXPECT_EQ(recycle_bin.items(), 12u) << "Die gelöschten Dateien gingen nicht in den Papierkorb.";
}

TEST_F(GuiTest, ZweiWegeKonfliktBehaeltBeideFassungen)  // [SyncOptionsTest.md#conflict-copies]
{
    const SandboxTree tree(GuiTestSandbox::root() / "konflikt", {{"quelle/text.txt", "v1"}, {"ziel/", ""}});
    age_file(tree, "quelle/text.txt");
    SyncItemInput input = dir_sync(tree, "guitest-konflikt");
    input.options.two_way_sync = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ASSERT_TRUE(sync_once(main_window()));
    ASSERT_EQ(tree.read("ziel/text.txt"), "v1");
    tree.write("quelle/text.txt", "Quelle v2");
    tree.write("ziel/text.txt", "Ziel v2, zuletzt");
    write_time_apart(tree, "ziel/text.txt", "quelle/text.txt", std::chrono::seconds(600));
    let_the_pause_elapse();

    ModalDialogWatcher::instance().answer(translation::translate("synchronisation") + L": " + translation::translate("warning"), wxID_OK);
    ASSERT_TRUE(sync_once(main_window()));

    EXPECT_EQ(tree.read("quelle/text.txt"), "Ziel v2, zuletzt") << "Die zuletzt geänderte Fassung gewinnt.";
    std::size_t copies = 0;
    for (const std::string& entry : tree.entries())
        if (entry.find("text (") != std::string::npos)
            ++copies;
    EXPECT_EQ(copies, 2u) << "Die Konfliktkopie fehlt auf einer Seite.";
    EXPECT_FALSE(ModalDialogWatcher::instance().has_pending_expectations()) << "Die Warnbox, die die Konfliktkopie nennt, fehlt.";
    EXPECT_TRUE(ModalDialogWatcher::instance().unexpected_dialogs().empty()) << "Ein unerwarteter Dialog erschien.";
}

}
