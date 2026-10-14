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
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "ProgressDialog.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"
#include <wx/gauge.h>
#include <filesystem>
#include <fstream>

using namespace AFS_Core;

using utilwx::tul;

namespace fs = std::filesystem;

namespace guitest {
namespace {

constexpr std::size_t BLOCK_BYTES = 1 << 20;
constexpr std::size_t HUGE_FILE_BLOCKS = 512;  // [HugeFileTest.md#HUGE_FILE_BLOCKS]
constexpr const char* HUGE_FILE = "riesig.bin";

std::string block(std::size_t index)  // [HugeFileTest.md#block]
{
    return std::string(BLOCK_BYTES, static_cast<char>(index));
}

void write_huge_file(const fs::path& file)
{
    std::ofstream out(file, std::ios::binary);
    for (std::size_t i = 0; i < HUGE_FILE_BLOCKS; ++i)
        out << block(i);
}

bool has_huge_content(const fs::path& file)  // [HugeFileTest.md#block]
{
    std::ifstream in(file, std::ios::binary);
    std::string bytes(BLOCK_BYTES, '\0');
    for (std::size_t i = 0; i < HUGE_FILE_BLOCKS; ++i) {
        if (!in.read(bytes.data(), BLOCK_BYTES) || bytes != block(i))
            return false;
    }
    return in.peek() == std::ifstream::traits_type::eof();
}

SyncItemInput dir_sync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

}

TEST_F(GuiTest, EinmalSynchronisierenKopiertRiesigeDateiMitFortschrittsdialog)  // [HugeFileTest.md#EinmalSynchronisierenKopiertRiesigeDateiMitFortschrittsdialog]
{
    const SandboxTree tree(GuiTestSandbox::root() / "riesig", {{"quelle/", ""}, {"ziel/", ""}});
    write_huge_file(tree.path("quelle") / HUGE_FILE);
    const auto written_at = fs::last_write_time(tree.path("quelle") / HUGE_FILE);
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-riesig")));

    activate_menu_item(main_window(), tul(id::SyncOnce));
    ASSERT_TRUE(wait_until([] { return progress_dialog() != nullptr; })) << "Der Fortschrittsdialog erschien nicht.";
    ProgressDialog& dialog = *progress_dialog();
    EXPECT_EQ(dialog.GetTitle(), translation::translate("progress_title"));
    EXPECT_TRUE(wait_until([&dialog] { return current_file_shown(dialog).Contains(HUGE_FILE); }))
        << "Der Dialog nennt die Datei nicht, zuletzt: " << current_file_shown(dialog);
    EXPECT_EQ(gauge_of(dialog)->GetRange(), 1) << "Der Balken ist nicht auf die eine Datei des Laufs eingestellt.";
    EXPECT_TRUE(find(dialog, wxID_CANCEL)->IsEnabled()) << "Abbrechen ist während der Kopie gesperrt.";

    ASSERT_TRUE(wait_until_sync_finished(main_window())) << "Die Kopie kam nicht zu Ende.";
    EXPECT_TRUE(wait_until([] { return progress_dialog() == nullptr; })) << "Der Fortschrittsdialog blieb nach der Kopie offen.";
    const fs::path copy = tree.path("ziel") / HUGE_FILE;
    ASSERT_TRUE(fs::exists(copy)) << "Die riesige Datei wurde nicht ins Ziel kopiert.";
    EXPECT_EQ(fs::file_size(copy), HUGE_FILE_BLOCKS * BLOCK_BYTES);
    EXPECT_TRUE(has_huge_content(copy)) << "Die Kopie weicht von der Quelle ab.";
    EXPECT_EQ(fs::last_write_time(copy), written_at) << "Die Kopie trägt nicht den Zeitstempel der Quelle.";
    EXPECT_FALSE(tree.exists(std::string("ziel/") + HUGE_FILE + ".bak")) << "Die Sicherungskopie blieb liegen.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce))) << "\"Einmal synchronisieren\" bleibt nach der Kopie gesperrt.";
}

TEST_F(GuiTest, AbbrechenWaehrendRiesigerDateiHinterlaesstKeineHalbeDatei)  // [HugeFileTest.md#AbbrechenWaehrendRiesigerDateiHinterlaesstKeineHalbeDatei]
{
    const SandboxTree tree(GuiTestSandbox::root() / "riesig-abbruch", {{"quelle/", ""}, {"ziel/", ""}});
    write_huge_file(tree.path("quelle") / HUGE_FILE);
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-riesig-abbruch")));

    activate_menu_item(main_window(), tul(id::SyncOnce));
    ASSERT_TRUE(wait_until([] { return progress_dialog() != nullptr; })) << "Der Fortschrittsdialog erschien nicht.";
    ASSERT_TRUE(click_on(find(*progress_dialog(), wxID_CANCEL))) << "Abbrechen ließ sich nicht klicken.";
    ASSERT_TRUE(wait_until_sync_finished(main_window())) << "Nach Abbrechen kam der Lauf nicht zur Ruhe.";

    EXPECT_TRUE(wait_until([] { return progress_dialog() == nullptr; })) << "Der Fortschrittsdialog blieb nach Abbrechen offen.";
    const fs::path copy = tree.path("ziel") / HUGE_FILE;
    if (fs::exists(copy))
        EXPECT_TRUE(has_huge_content(copy)) << "Nach Abbrechen liegt eine unvollständige Datei im Ziel.";
    EXPECT_FALSE(tree.exists(std::string("ziel/") + HUGE_FILE + ".bak")) << "Die Sicherungskopie blieb nach Abbrechen liegen.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce))) << "Nach Abbrechen bleibt \"Einmal synchronisieren\" gesperrt.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::StartSync)));
}

}
