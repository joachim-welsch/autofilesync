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

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

constexpr int MANY_FILES = 1500;  // [ProgressDialogTest.md#MANY_FILES]

FileContents many_files()
{
    FileContents files{{"ziel/", ""}};
    for (int i = 0; i < MANY_FILES; ++i)
        files.push_back({"quelle/datei" + std::to_string(i) + ".txt", std::string(4096, 'x')});
    return files;
}

SyncItemInput dir_sync(const SandboxTree& tree, const wxString& name)
{
    SyncItemInput input{name, tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    return input;
}

std::size_t copied_files(const SandboxTree& tree)
{
    std::size_t count = 0;
    for (const std::string& entry : tree.entries())
        count += entry.starts_with("ziel/") ? 1 : 0;
    return count;
}

}

TEST_F(GuiTest, FortschrittsdialogZeigtAktuelleDateiUndSchliesstSich)  // [ProgressDialogTest.md#FortschrittsdialogZeigtAktuelleDateiUndSchliesstSich]
{
    const SandboxTree tree(GuiTestSandbox::root() / "fortschritt", many_files());
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-fortschritt")));

    activate_menu_item(main_window(), tul(id::SyncOnce));
    ASSERT_TRUE(wait_until([] { return progress_dialog() != nullptr; })) << "Der Fortschrittsdialog erschien nicht.";
    ProgressDialog& dialog = *progress_dialog();
    EXPECT_EQ(dialog.GetTitle(), translation::translate("progress_title"));
    EXPECT_TRUE(wait_until([&dialog] { return current_file_shown(dialog).Contains("datei"); }))
        << "Der Dialog zeigt keine Datei an, zuletzt: " << current_file_shown(dialog);
    EXPECT_TRUE(find(dialog, wxID_CANCEL)->IsEnabled()) << "Abbrechen ist während des Laufs gesperrt.";

    ASSERT_TRUE(wait_until_sync_finished(main_window()));
    EXPECT_TRUE(wait_until([] { return progress_dialog() == nullptr; })) << "Der Fortschrittsdialog blieb nach dem Lauf offen.";
    EXPECT_EQ(copied_files(tree), MANY_FILES);
}

TEST_F(GuiTest, AbbrechenImFortschrittsdialogBeendetDenLauf)  // [ProgressDialogTest.md#AbbrechenImFortschrittsdialogBeendetDenLauf]
{
    const SandboxTree tree(GuiTestSandbox::root() / "abbruch", many_files());
    ASSERT_TRUE(add_sync_item(main_window(), dir_sync(tree, "guitest-abbruch")));

    activate_menu_item(main_window(), tul(id::SyncOnce));
    ASSERT_TRUE(wait_until([] { return progress_dialog() != nullptr; })) << "Der Fortschrittsdialog erschien nicht.";
    ASSERT_TRUE(click_on(find(*progress_dialog(), wxID_CANCEL))) << "Abbrechen ließ sich nicht klicken.";
    ASSERT_TRUE(wait_until_sync_finished(main_window())) << "Nach Abbrechen kam der Lauf nicht zur Ruhe.";

    const std::size_t copied = copied_files(tree);
    EXPECT_LT(copied, MANY_FILES) << "Abbrechen hat den Lauf nicht unterbrochen.";
    EXPECT_TRUE(wait_until([] { return progress_dialog() == nullptr; })) << "Der Fortschrittsdialog blieb nach Abbrechen offen.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::SyncOnce))) << "Nach Abbrechen bleibt \"Einmal synchronisieren\" gesperrt.";
    EXPECT_TRUE(sync_menu_item_enabled(main_window(), tul(id::StartSync)));
}

}
