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
#include "SshTestServer.h"
#include "EventPump.h"
#include "ModalDialogWatcher.h"
#include "UserActions.h"
#include "SshDirDialog.h"
#include "SshDirInspector.h"
#include "TextResources.h"
#include <wx/listctrl.h>
#include <array>
#include <string>
#include <vector>

using namespace AFS_Core;

namespace guitest {
namespace {

constexpr std::array<const char*, 3> SORT_PROBE_FILES_UNSORTED = {"zeta.txt", "alpha.txt", "mango.txt"};

}

class SshDirDialogTest: public SshGuiTest {  // [SshDirDialogTest.md#SshDirDialogTest]
protected:
    void SetUp() override
    {
        SshGuiTest::SetUp();
        if (IsSkipped())
            return;
        for (const char* name : SORT_PROBE_FILES_UNSORTED)
            ASSERT_TRUE(sftp().create_file(probe_dir() + "/" + name)) << "Anlegen der Testdatei fehlgeschlagen: " << name;
    }

    void TearDown() override
    {
        if (!IsSkipped())
            for (const char* name : SORT_PROBE_FILES_UNSORTED)
                sftp().remove_file(probe_dir() + "/" + name);
        SshGuiTest::TearDown();
    }
};

TEST_F(SshDirDialogTest, HomeButtonNavigatesToLoginDirectory)
{
    const SshAccount account = ssh_test_account("guitest-sshdirdialog");
    const wxString expected_home = wxString::FromUTF8(SshDirInspector(account).get_home_dir());

    ModalDialogWatcher::instance().expect(txt::select_ssh_dir(), [&](wxDialog& dialog) {
        wxWindow* home_button = find(dialog, ID_SSH_DIR_HOME_BUTTON);
        wxWindow* path_box = find(dialog, ID_SSH_DIR_PATH_BOX);

        ASSERT_TRUE(click_on(home_button));
        ASSERT_TRUE(wait_until([&] { return value_of(path_box) == expected_home; }));
        EXPECT_EQ(value_of(path_box), expected_home);

        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dlg(&main_window(), txt::select_ssh_dir(), account, wxString::FromUTF8(probe_dir()));  // [SshDirDialogTest.md#home-start-dir]
    dlg.ShowModal();
}

TEST_F(SshDirDialogTest, SortButtonTogglesAscendingAndDescending)
{
    const SshAccount account = ssh_test_account("guitest-sshdirdialog");

    ModalDialogWatcher::instance().expect(txt::select_ssh_dir(), [](wxDialog& dialog) {
        wxWindow* sort_button = find(dialog, ID_SSH_DIR_SORT_BUTTON);
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSH_DIR_LIST));
        ASSERT_NE(list, nullptr);
        ASSERT_TRUE(wait_until([&] { return list->GetItemCount() == 3; }));

        auto names_in_order = [&] {
            std::vector<wxString> names;
            for (int i = 0; i < list->GetItemCount(); ++i)
                names.push_back(list->GetItemText(i));
            return names;
        };

        ASSERT_TRUE(click_on(sort_button));
        pump(100);
        EXPECT_EQ(names_in_order(), (std::vector<wxString>{"alpha.txt", "mango.txt", "zeta.txt"}));

        ASSERT_TRUE(click_on(sort_button));
        pump(100);
        EXPECT_EQ(names_in_order(), (std::vector<wxString>{"zeta.txt", "mango.txt", "alpha.txt"}));

        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dlg(&main_window(), txt::select_ssh_dir(), account, wxString::FromUTF8(probe_dir()));
    dlg.ShowModal();
}

}
