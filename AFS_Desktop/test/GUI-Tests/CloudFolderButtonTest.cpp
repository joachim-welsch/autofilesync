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
#include "UserActions.h"
#include "SyncItemDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/choicdlg.h>
#include <wx/listbox.h>
#include <filesystem>
#include <fstream>
#include <functional>

using namespace AFS_Core;
namespace fs = std::filesystem;

using utilwx::tul;

namespace guitest {
namespace {

constexpr wchar_t DROPBOX[] = L"Dropbox";
constexpr wchar_t NEXTCLOUD[] = L"Nextcloud";

fs::path dropbox_info_file()
{
#ifdef _WIN32
    return GuiTestSandbox::root() / "AppData" / "Dropbox" / "info.json";
#else
    return GuiTestSandbox::root() / ".dropbox" / "info.json";
#endif
}

fs::path nextcloud_config_file()
{
#ifdef _WIN32
    return GuiTestSandbox::root() / "AppData" / "Nextcloud" / "nextcloud.cfg";
#elif defined(_MACOS)
    return GuiTestSandbox::root() / "Library" / "Preferences" / "Nextcloud" / "nextcloud.cfg";
#else
    return GuiTestSandbox::root() / ".config" / "Nextcloud" / "nextcloud.cfg";
#endif
}

wxString dir_dialog_title(const wchar_t* provider)
{
    return translation::translate("choose_cloud_dir_title", {provider});
}

wxListBox* list_of(wxDialog& dialog)
{
    for (wxWindow* child : dialog.GetChildren())
        if (auto* list = dynamic_cast<wxListBox*>(child))
            return list;
    return nullptr;
}

class CloudFolderButtonTest: public GuiTest {  // [CloudFolderButtonTest.md#CloudFolderButtonTest]
protected:
    std::vector<fs::path> created_;

    fs::path make_backup_dir_in(const char* cloud_folder_name)
    {
        const fs::path cloud_folder = GuiTestSandbox::root() / cloud_folder_name;
        fs::create_directories(cloud_folder / "Sicherung");
        created_.push_back(cloud_folder);
        return fs::canonical(cloud_folder / "Sicherung");
    }

    void write(const fs::path& file, const std::string& content)
    {
        created_.push_back(file.parent_path());
        fs::create_directories(file.parent_path());
        std::ofstream(file) << content;
    }

    void install_dropbox()
    {
        write(dropbox_info_file(), "{\"personal\": {\"path\": \"" + (GuiTestSandbox::root() / "Dropbox").generic_string() + "\"}}");
    }

    void install_nextcloud()
    {
        write(nextcloud_config_file(), "[Accounts]\n0\\Folders\\1\\localPath=" + (GuiTestSandbox::root() / "Nextcloud").generic_string() + "/\n");
    }

    wxString target_after_cloud_button(const std::function<void(ModalDialogWatcher&)>& expect_follow_ups)  // [CloudFolderButtonTest.md#target_after_cloud_button]
    {
        wxString target;
        ModalDialogWatcher::instance().expect(txt::add_synchronisation(), [&target](wxDialog& dialog) {
            EXPECT_TRUE(wait_until([&dialog] { return dialog.IsShownOnScreen(); })) << "Der Dialog erschien nicht.";
            EXPECT_TRUE(click_on(find(dialog, ID_BTN_CLOUD_TARGET))) << "Der Cloud-Knopf ließ sich nicht anklicken.";
            pump();
            target = value_of(find(dialog, ID_TARGET_DIR_BOX));
            dialog.EndModal(wxID_CANCEL);
        });
        expect_follow_ups(ModalDialogWatcher::instance());
        activate_menu_item(main_window(), tul(id::File_new_Synchronization));
        pump();
        return target;
    }

    void TearDown() override
    {
        for (const fs::path& path : created_)
            fs::remove_all(path);
        GuiTest::TearDown();
    }
};

}

TEST_F(CloudFolderButtonTest, OhneCloudOrdnerNenntDerKnopfWasFehlt)  // [CloudFolderButtonTest.md#needs-clean-machine]
{
    const wxString target = target_after_cloud_button([](ModalDialogWatcher& watcher) {
        watcher.answer(translation::translate("info"), wxID_OK);
    });
    EXPECT_TRUE(target.empty()) << "Ohne Cloud-Ordner wurde ein Ziel eingetragen.";
    const auto& messages = ModalDialogWatcher::instance().answered_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages.front(), wxString(translation::translate("no_cloud_folder_found")));
}

TEST_F(CloudFolderButtonTest, EinzigerCloudOrdnerFuehrtGleichZurVerzeichniswahl)
{
    const fs::path backup_dir = make_backup_dir_in("Dropbox");
    install_dropbox();
    const wxString target = target_after_cloud_button([&](ModalDialogWatcher& watcher) {
        watcher.choose_path(dir_dialog_title(DROPBOX), backup_dir.wstring());
    });
    EXPECT_EQ(target, wxString(backup_dir.wstring()));
}

TEST_F(CloudFolderButtonTest, MehrereCloudOrdnerStehenZurWahl)
{
    make_backup_dir_in("Dropbox");
    const fs::path backup_dir = make_backup_dir_in("Nextcloud");
    install_dropbox();
    install_nextcloud();
    const wxString target = target_after_cloud_button([&](ModalDialogWatcher& watcher) {
        watcher.expect(txt::cloud_folder(), [](wxDialog& dialog) {
            wxListBox* list = list_of(dialog);
            ASSERT_NE(list, nullptr);
            ASSERT_EQ(list->GetCount(), 2u);
            EXPECT_TRUE(list->GetString(0).StartsWith(DROPBOX)) << list->GetString(0);
            EXPECT_TRUE(list->GetString(1).StartsWith(NEXTCLOUD)) << list->GetString(1);
            static_cast<wxSingleChoiceDialog&>(dialog).SetSelection(1);
            EXPECT_TRUE(click_when_enabled(find(dialog, wxID_OK)));
        });
        watcher.choose_path(dir_dialog_title(NEXTCLOUD), backup_dir.wstring());
    });
    EXPECT_EQ(target, wxString(backup_dir.wstring()));
}

TEST_F(CloudFolderButtonTest, AbgebrocheneWahlLaesstDasZielUnveraendert)
{
    make_backup_dir_in("Dropbox");
    make_backup_dir_in("Nextcloud");
    install_dropbox();
    install_nextcloud();
    const wxString target = target_after_cloud_button([](ModalDialogWatcher& watcher) {
        watcher.answer(txt::cloud_folder(), wxID_CANCEL);
    });
    EXPECT_TRUE(target.empty()) << "Trotz Abbruch wurde ein Ziel eingetragen.";
}

}
