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

const wxString passwordWithSpecialCharacters = R"(geheim$+!"#%&/()=?@[]{}\<>|,.;:-_*'^~`)";  // [AddSshAccountTest.md#passwordWithSpecialCharacters]
const wxString accountName = "guitest-ssh";

}

TEST_F(GuiTest, SshPasswortMitSonderzeichenWirdUebernommen)
{
    const SshAccountInput input{accountName, "localhost", "pi", passwordWithSpecialCharacters};
    ModalDialogWatcher::instance().expect(txt::SshAccounts, addSshAccountAndClose());
    ModalDialogWatcher::instance().expect(txt::AddSshConnection, fillSshAccountDialog(input));

    activateMenuItem(mainWindow(), tul(id::EditSshAccounts));
    pump();

    const auto created = Config::instance().find_ssh_account_by_name(accountName.ToStdString());
    ASSERT_TRUE(created.has_value()) << "Die SSH-Verbindung wurde nicht angelegt.";
    EXPECT_EQ(created->password(), std::string(passwordWithSpecialCharacters.ToUTF8()));
}

}
