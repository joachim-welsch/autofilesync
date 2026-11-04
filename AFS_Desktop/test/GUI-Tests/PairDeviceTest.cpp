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
#include "Config.h"
#include "DeviceLink.h"
#include "DevicesDialog.h"
#include "FirewallHint.h"
#include "PairDeviceDialog.h"
#include "PhoneTemplatesDialog.h"
#include "FakePhone.h"
#include <wx/filepicker.h>
#include <fstream>
#ifndef _WIN32  // the lock-holding test below uses flock
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/listctrl.h>
#include <wx/statbmp.h>

using namespace AFS_Core;
using utilwx::tul;

namespace guitest {
namespace {

const std::string PHONE_ID = "fedcba9876543210fedcba9876543210";
const wxString PHONE_NAME = "GUI-Test-Handy";

template<class Control> Control* find_control(wxWindow& parent, const std::function<bool(Control&)>& wanted = [](Control&) { return true; })
{
    for (wxWindow* child : parent.GetChildren()) {
        if (auto* control = dynamic_cast<Control*>(child); control && wanted(*control))
            return control;
        if (Control* found = find_control<Control>(*child, wanted))
            return found;
    }
    return nullptr;
}

bool label_shown(wxWindow& dialog, const wxString& text)
{
    return find_control<wxStaticText>(dialog, [&](wxStaticText& label) { return label.IsShown() && label.GetLabel().Contains(text); }) != nullptr;
}

class PairDeviceTest : public GuiTest {  // [PairDeviceTest.md#PairDeviceTest]
protected:
    void TearDown() override
    {
        DeviceLink::instance().stop();
        Config::instance().remove_device(PHONE_ID);
        GuiTest::TearDown();
    }
};

}

TEST_F(PairDeviceTest, HandyVerbindenZeigtCodeUndNimmtKopplungAn)  // [PairDeviceTest.md#pairing]
{
    ModalDialogWatcher::instance().expect(txt::connect_phone(), [](wxDialog& dialog) {
        DeviceLink& link = DeviceLink::instance();
        ASSERT_TRUE(link.running()) << "Der Dialog muss die Verbindung für Handys öffnen.";
        EXPECT_TRUE(link.pairing().open()) << "Mit dem Dialog muss ein Kopplungs-Token offen sein.";
        wxStaticBitmap* code = find_control<wxStaticBitmap>(dialog);
        ASSERT_NE(code, nullptr);
        EXPECT_TRUE(code->IsShown());
        EXPECT_GT(code->GetBitmap().GetWidth(), 100) << "Der QR-Code wurde nicht gezeichnet.";
        EXPECT_TRUE(label_shown(dialog, translation::translate("pair_waiting")));
        EXPECT_TRUE(label_shown(dialog, translation::translate("pair_same_network"))) << "Der Dialog muss sagen, dass kein USB-Kabel gebraucht wird.";
        const tu::FakePhone phone(GuiTestSandbox::root() / "phone");
        const auto reply = phone.send(link.port(), tu::FakePhone::pair_line(link.pairing().token(), PHONE_ID, std::string(PHONE_NAME.ToUTF8())));
        EXPECT_EQ(reply.type(), "paired") << (reply.lines.empty() ? std::string("keine Antwort") : reply.lines[0]);
        EXPECT_TRUE(wait_until([&] { return label_shown(dialog, PHONE_NAME); })) << "Der Dialog zeigt das verbundene Handy nicht an.";
        EXPECT_FALSE(code->IsShown()) << "Nach der Kopplung hat der Code nichts mehr zu suchen.";
        dialog.EndModal(wxID_CLOSE);
    });
    ModalDialogWatcher::instance().expect(txt::phone_templates(), [](wxDialog& dialog) { dialog.EndModal(wxID_CANCEL); });  // the second page follows a pairing

    activate_menu_item(main_window(), tul(id::File_connect_phone));
    pump();

    const auto device = Config::instance().find_device(PHONE_ID);
    ASSERT_TRUE(device.has_value()) << "Das Handy kam nicht in die Konfiguration.";
    EXPECT_EQ(wxString::FromUTF8(device->name), PHONE_NAME);
    EXPECT_FALSE(DeviceLink::instance().pairing().open()) << "Nach dem Dialog darf kein Token mehr offen sein.";
    EXPECT_TRUE(DeviceLink::instance().running()) << "Mit einem verbundenen Handy bleibt die Verbindung offen.";
}

TEST_F(PairDeviceTest, DialogOhneHandySchliesstDieVerbindungWieder)  // [PairDeviceTest.md#no-port-without-phone]
{
    ModalDialogWatcher::instance().expect(txt::connect_phone(), [](wxDialog& dialog) {
        EXPECT_TRUE(DeviceLink::instance().running());
        dialog.EndModal(wxID_CLOSE);
    });

    activate_menu_item(main_window(), tul(id::File_connect_phone));
    pump();

    EXPECT_FALSE(DeviceLink::instance().running()) << "Ohne verbundenes Handy darf kein Port offen bleiben.";
}

TEST_F(PairDeviceTest, FirewallHinweisErscheintOhneHandy)  // [PairDeviceTest.md#firewall-hint]
{
    ModalDialogWatcher::instance().expect(txt::connect_phone(), [](wxDialog& dialog) {
        auto* command = dynamic_cast<wxTextCtrl*>(find(dialog, ID_PAIRDEVICE_FIREWALL_COMMAND));
        ASSERT_NE(command, nullptr);
        EXPECT_FALSE(command->IsShown()) << "Der Befehl gehört erst zum Hinweis.";
        const std::wstring port = std::to_wstring(DeviceLink::instance().port());
        const wxString hint = ufw_blocks_incoming() ? translation::translate("pair_ufw_hint", {port}) : translation::translate("pair_firewall_hint", {port});
        EXPECT_TRUE(wait_until([&] { return label_shown(dialog, hint.BeforeFirst(' ')); }, 20000)) << "Nach 15 Sekunden ohne Handy fehlt der Firewall-Hinweis.";
        if (ufw_blocks_incoming()) {
            EXPECT_TRUE(command->IsShown());
            EXPECT_TRUE(command->GetValue().Contains("sudo ufw allow") && command->GetValue().Contains(wxString(port))) << command->GetValue();
        }
        dialog.EndModal(wxID_CLOSE);
    });

    activate_menu_item(main_window(), tul(id::File_connect_phone));
    pump();
}

TEST(FirewallHint, ErkenntUfwAusSeinenEinstellungen)  // [PairDeviceTest.md#ufw-settings]
{
    const auto etc = std::filesystem::temp_directory_path() / "afs-firewallhint-test";
    std::filesystem::create_directories(etc / "ufw");
    std::filesystem::create_directories(etc / "default");
    const auto write = [](const std::filesystem::path& file, const std::string& text) { std::ofstream(file) << text; };
    EXPECT_FALSE(ufw_blocks_incoming(etc)) << "Ohne ufw blockiert ufw nichts.";
    write(etc / "ufw" / "ufw.conf", "# comment\nENABLED=yes\nLOGLEVEL=low\n");
    write(etc / "default" / "ufw", "DEFAULT_INPUT_POLICY=\"DROP\"\n");
    EXPECT_TRUE(ufw_blocks_incoming(etc));
    write(etc / "default" / "ufw", "DEFAULT_INPUT_POLICY=\"ACCEPT\"\n");
    EXPECT_FALSE(ufw_blocks_incoming(etc)) << "Alles erlaubt: nichts zu tun.";
    write(etc / "ufw" / "ufw.conf", "ENABLED=no\n");
    EXPECT_FALSE(ufw_blocks_incoming(etc));
    std::filesystem::remove_all(etc);

    EXPECT_EQ(ufw_allow_command({}, 52381), "sudo ufw allow 52381");
    EXPECT_EQ(ufw_allow_command({"192.168.1.0/24", "10.0.0.0/8"}, 52381),
              "sudo ufw allow from 192.168.1.0/24 to any port 52381 && sudo ufw allow from 10.0.0.0/8 to any port 52381");
    for (const auto& network : local_ipv4_networks())
        EXPECT_NE(network.find('/'), std::string::npos) << network;
}

TEST_F(PairDeviceTest, HandysDialogListetUndTrennt)  // [PairDeviceTest.md#unpair]
{
    Config::instance().add(Device{PHONE_ID, std::string(PHONE_NAME.ToUTF8()), std::string(64, 'a'), "ssh-ed25519 AAAA"});
    ModalDialogWatcher::instance().expect(txt::devices(), [](wxDialog& dialog) {  // expectations are matched in order: the list dialog opens before the question
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_DEVICES_LIST));
        ASSERT_NE(list, nullptr);
        ASSERT_EQ(list->GetItemCount(), 1);
        EXPECT_EQ(list->GetItemText(0), PHONE_NAME);
        EXPECT_EQ(list->GetItemText(0, 1), translation::translate("device_never_seen"));
        EXPECT_TRUE(click_on(*list, 0));
        EXPECT_TRUE(click_when_enabled(find(dialog, ID_DEVICES_UNPAIR)));
        EXPECT_TRUE(wait_until([] { return !Config::instance().find_device(PHONE_ID).has_value(); })) << "Das Handy wurde nicht getrennt.";
        EXPECT_EQ(list->GetItemCount(), 0);
        dialog.EndModal(wxID_CLOSE);
    });
    ModalDialogWatcher::instance().answer(translation::translate("confirm_delete_title"), wxID_YES);

    activate_menu_item(main_window(), tul(id::EditDevices));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Das Trennen muss nachfragen.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answered_messages().front().Contains(PHONE_NAME));
    EXPECT_FALSE(Config::instance().find_device(PHONE_ID).has_value());
}

TEST_F(PairDeviceTest, VorlagenSetzenSichereOptionen)  // [PairDeviceTest.md#templates]
{
    const Device device{PHONE_ID, std::string(PHONE_NAME.ToUTF8()), std::string(64, 'a'), "ssh-ed25519 AAAA"};
    const wxString pc = GuiTestSandbox::target_dir().string();

    const auto photos = phone_template_item(PhoneTemplate::Photos, device, pc, "");
    ASSERT_TRUE(photos.has_value());
    EXPECT_EQ(photos->source(), wxString::FromUTF8(PHONE_ID) + ":/DCIM/Camera");
    EXPECT_EQ(photos->target(), pc);
    EXPECT_EQ(photos->sync_item_type(), SyncItemType::Tree);
    EXPECT_FALSE(photos->delete_in_target()) << "Ein auf dem Handy gelöschtes Foto darf auf dem PC nicht verschwinden.";
    EXPECT_FALSE(photos->two_way_sync());
    EXPECT_FALSE(photos->warn_if_unreachable()) << "Ein abwesendes Handy ist der Normalfall.";

    const auto folder = phone_template_item(PhoneTemplate::Folder, device, pc, "/Documents");
    ASSERT_TRUE(folder.has_value());
    EXPECT_TRUE(folder->two_way_sync());
    EXPECT_TRUE(folder->conflict_copies());
    EXPECT_TRUE(folder->track_moves());

    const auto music = phone_template_item(PhoneTemplate::Music, device, pc, "");
    ASSERT_TRUE(music.has_value());
    EXPECT_EQ(music->source(), pc);
    EXPECT_EQ(music->target(), wxString::FromUTF8(PHONE_ID) + ":/Music");
    EXPECT_TRUE(music->delete_in_target()) << "Musik, die der PC nicht mehr hat, soll das Handy nicht füllen.";

    EXPECT_FALSE(phone_template_item(PhoneTemplate::None, device, pc, "").has_value());
    EXPECT_FALSE(phone_template_item(PhoneTemplate::Photos, device, "", "").has_value()) << "Ohne PC-Ordner gibt es keine Synchronisation.";
}

TEST_F(PairDeviceTest, HandysDialogRichtetSynchronisationEin)  // [PairDeviceTest.md#templates]
{
    Config::instance().add(Device{PHONE_ID, std::string(PHONE_NAME.ToUTF8()), std::string(64, 'a'), "ssh-ed25519 AAAA"});
    const std::wstring expected_name = translation::translate("item_name_photos", {PHONE_NAME.ToStdWstring()});
    ModalDialogWatcher::instance().expect(txt::devices(), [](wxDialog& dialog) {
        EXPECT_TRUE(click_on(find(dialog, ID_DEVICES_SETUP)));
        dialog.EndModal(wxID_CLOSE);
    });
    ModalDialogWatcher::instance().expect(txt::phone_templates(), [&](wxDialog& dialog) {
        auto* pc_folder = dynamic_cast<wxDirPickerCtrl*>(find(dialog, ID_TEMPLATES_PC_FOLDER));
        ASSERT_NE(pc_folder, nullptr);
        pc_folder->SetPath(GuiTestSandbox::target_dir().string());
        EXPECT_TRUE(click_on(find(dialog, wxID_OK)));
    });

    activate_menu_item(main_window(), tul(id::EditDevices));
    pump();

    const auto item = Config::instance().find_by_name(expected_name);
    ASSERT_TRUE(item.has_value()) << "Die Vorlage hat keine Synchronisation angelegt.";
    EXPECT_EQ(item->source(), wxString::FromUTF8(PHONE_ID) + ":/DCIM/Camera");
    Config::instance().erase_by_name(expected_name);
}


#ifndef _WIN32
TEST_F(PairDeviceTest, DialogZeigtCodeAuchWennDerDienstLauscht)  // [PairDeviceTest.md#hosted-elsewhere]
{
    const std::string lock = (std::filesystem::temp_directory_path() / ("autofilesync-" + std::to_string(getuid()) + "-test-devicelink.lock")).string();
    const int held = open(lock.c_str(), O_RDWR | O_CREAT, 0600);  // the background service holds the role
    ASSERT_EQ(flock(held, LOCK_EX | LOCK_NB), 0);
    const std::filesystem::path port_file = DeviceLink::default_dir() / "devicelink.port";
    std::ofstream(port_file) << "47111\n";
    ModalDialogWatcher::instance().expect(txt::connect_phone(), [](wxDialog& dialog) {
        EXPECT_FALSE(DeviceLink::instance().running()) << "Der Dialog darf dem Dienst die Rolle nicht nehmen.";
        wxStaticBitmap* code = find_control<wxStaticBitmap>(dialog);
        ASSERT_NE(code, nullptr);
        EXPECT_TRUE(code->IsShown()) << "Auch ohne eigenen Port muss der Code erscheinen.";
        EXPECT_TRUE(label_shown(dialog, "47111")) << "Der Code muss den Port des Dienstes nennen.";
        EXPECT_TRUE(DeviceLink::instance().pairing().open()) << "Der Token muss für den Dienst bereitliegen.";
        dialog.EndModal(wxID_CLOSE);
    });

    activate_menu_item(main_window(), tul(id::File_connect_phone));
    pump();

    EXPECT_FALSE(DeviceLink::instance().pairing().open()) << "Mit dem Dialog endet der Token auch für den Dienst.";
    std::filesystem::remove(port_file);
    flock(held, LOCK_UN);
    close(held);
}
#endif

}
