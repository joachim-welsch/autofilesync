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
#include "SshAccountDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "id.h"
#include "TextResources.h"
#include "utilwx.h"

using namespace AFS_Core;

using utilwx::tul;

namespace guitest {
namespace {

const wxString PASSWORD_WITH_SPECIAL_CHARACTERS = R"(geheim$+!"#%&/()=?@[]{}\<>|,.;:-_*'^~`)";  // [AddSshAccountTest.md#PASSWORD_WITH_SPECIAL_CHARACTERS]
const wxString account_name = "guitest-ssh";

}

TEST_F(GuiTest, SshPasswortMitSonderzeichenWirdUebernommen)
{
    const SshAccountInput input{account_name, "localhost", "pi", PASSWORD_WITH_SPECIAL_CHARACTERS};
    ModalDialogWatcher::instance().expect(txt::ssh_connections(), add_ssh_account_and_close());
    ModalDialogWatcher::instance().expect(txt::add_ssh_connection(), fill_ssh_account_dialog(input));

    activate_menu_item(main_window(), tul(id::EditSshAccounts));
    pump();

    const auto created = Config::instance().find_ssh_account_by_name(account_name.ToStdString());
    ASSERT_TRUE(created.has_value()) << "Die SSH-Verbindung wurde nicht angelegt.";
    EXPECT_EQ(created->password(), std::string(PASSWORD_WITH_SPECIAL_CHARACTERS.ToUTF8()));
}

}
