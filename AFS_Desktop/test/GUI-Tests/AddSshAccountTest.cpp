/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
