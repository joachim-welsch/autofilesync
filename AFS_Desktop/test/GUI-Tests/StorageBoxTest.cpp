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
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "Config.h"
#include "SshAccountDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {

TEST_F(GuiTest, StorageBoxBenutzernameBestimmtDieServeradresse)  // [StorageBoxTest.md#names]
{
    EXPECT_TRUE(storage_box_user("u123456"));
    EXPECT_TRUE(storage_box_user("u123456-sub12"));
    for (const char* wrong : {"", "u", "123456", "anna", "u12a", "u123456-sub", "u123456-subx", "u123456-sub1-sub2", "u123456.your-storagebox.de"})
        EXPECT_FALSE(storage_box_user(wrong)) << wrong;
    EXPECT_EQ(storage_box_host("u123456-sub12"), "u123456-sub12.your-storagebox.de");
}

TEST_F(GuiTest, StorageBoxDialogFragtNichtNachHostUndLehntFremdeNamenAb)  // [StorageBoxTest.md#dialog]
{
    ModalDialogWatcher::instance().expect(txt::add_storage_box_connection(), [](wxDialog& dialog) {
        ASSERT_TRUE(wait_until([&dialog] { return dialog.IsShownOnScreen(); }));
        EXPECT_FALSE(find(dialog, ID_SSH_HOST_BOX)->IsShown()) << "Der Dialog fragt nach dem Host.";
        EXPECT_FALSE(find(dialog, ID_SSH_PORT_BOX)->IsShown()) << "Der Dialog fragt nach dem Port.";
        ASSERT_TRUE(replace_text(find(dialog, ID_SSH_NAME_BOX), "guitest-storagebox"));
        ASSERT_TRUE(replace_text(find(dialog, ID_SSH_USER_BOX), "anna"));
        ASSERT_TRUE(replace_text(find(dialog, ID_SSH_PASSWORD_BOX), "geheim"));
        ModalDialogWatcher::instance().answer(translation::translate("validation_error"), wxID_OK);
        EXPECT_TRUE(click_when_enabled(find(dialog, wxID_OK)));
        pump();
        EXPECT_TRUE(dialog.IsModal()) << "Der Dialog schloss sich trotz ungültigem Benutzernamen.";
        if (dialog.IsModal())
            dialog.EndModal(wxID_CANCEL);
    });
    activate_menu_item(main_window(), tul(id::File_new_StorageBoxConnection));
    pump();
    EXPECT_FALSE(Config::instance().find_ssh_account_by_name("guitest-storagebox").has_value());
    const auto& messages = ModalDialogWatcher::instance().answered_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages.front(), wxString(translation::translate("storage_box_user_invalid")));
}

}
