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
            ASSERT_TRUE(sftp().createFile(probeDir() + "/" + name)) << "Anlegen der Testdatei fehlgeschlagen: " << name;
    }

    void TearDown() override
    {
        if (!IsSkipped())
            for (const char* name : SORT_PROBE_FILES_UNSORTED)
                sftp().removeFile(probeDir() + "/" + name);
        SshGuiTest::TearDown();
    }
};

TEST_F(SshDirDialogTest, HomeButtonNavigatesToLoginDirectory)
{
    const SshAccount account = sshTestAccount("guitest-sshdirdialog");
    const wxString expectedHome = wxString::FromUTF8(SshDirInspector(account).get_home_dir());

    ModalDialogWatcher::instance().expect(L"SSH-Verzeichnis wählen", [&](wxDialog& dialog) {
        wxWindow* homeButton = find(dialog, ID_SSH_DIR_HOME_BUTTON);
        wxWindow* pathBox = find(dialog, ID_SSH_DIR_PATH_BOX);

        ASSERT_TRUE(clickOn(homeButton));
        ASSERT_TRUE(waitUntil([&] { return valueOf(pathBox) == expectedHome; }));
        EXPECT_EQ(valueOf(pathBox), expectedHome);

        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dlg(&mainWindow(), L"SSH-Verzeichnis wählen", account, wxString::FromUTF8(probeDir()));  // [SshDirDialogTest.md#home-start-dir]
    dlg.ShowModal();
}

TEST_F(SshDirDialogTest, SortButtonTogglesAscendingAndDescending)
{
    const SshAccount account = sshTestAccount("guitest-sshdirdialog");

    ModalDialogWatcher::instance().expect(L"SSH-Verzeichnis wählen", [](wxDialog& dialog) {
        wxWindow* sortButton = find(dialog, ID_SSH_DIR_SORT_BUTTON);
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_SSH_DIR_LIST));
        ASSERT_NE(list, nullptr);
        ASSERT_TRUE(waitUntil([&] { return list->GetItemCount() == 3; }));

        auto namesInOrder = [&] {
            std::vector<wxString> names;
            for (int i = 0; i < list->GetItemCount(); ++i)
                names.push_back(list->GetItemText(i));
            return names;
        };

        ASSERT_TRUE(clickOn(sortButton));
        pump(100);
        EXPECT_EQ(namesInOrder(), (std::vector<wxString>{"alpha.txt", "mango.txt", "zeta.txt"}));

        ASSERT_TRUE(clickOn(sortButton));
        pump(100);
        EXPECT_EQ(namesInOrder(), (std::vector<wxString>{"zeta.txt", "mango.txt", "alpha.txt"}));

        dialog.EndModal(wxID_CANCEL);
    });

    SshDirDialog dlg(&mainWindow(), L"SSH-Verzeichnis wählen", account, wxString::FromUTF8(probeDir()));
    dlg.ShowModal();
}

}
