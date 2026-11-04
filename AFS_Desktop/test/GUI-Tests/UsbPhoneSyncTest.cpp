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
#include "UsbPhone.h"
#include "Config.h"
#include "DeviceLink.h"
#include "DevicePairing.h"
#include "PairDeviceDialog.h"
#include "PhoneTemplatesDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/filepicker.h>
#include <fstream>
#include <random>

using namespace AFS_Core;
using utilwx::tul;

namespace guitest {
namespace {

constexpr uint16_t PHONE_SFTP_PORT = 52382;  // SftpServer.PREFERRED_PORT in the app [UsbPhoneSyncTest.md#sftp-port]
constexpr auto PHONE_TIMEOUT = std::chrono::seconds(20);
constexpr int PASS_TIMEOUT_MILLISECONDS = 120000;
const std::string FROM_PHONE = "vom-handy.txt";
const std::string FROM_PC = "vom-pc.txt";

std::string unique_phone_folder()  // [UsbPhoneSyncTest.md#phone-folder]
{
    std::mt19937 random(std::random_device{}());
    return "/AFS-GUI-Test-" + std::to_string(random());
}

std::string read_text(const std::filesystem::path& file)
{
    std::ifstream in(file);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::optional<Device> only_device()
{
    const auto devices = Config::instance().devices_snapshot();
    return devices.size() == 1 ? std::optional(devices.front()) : std::nullopt;
}

class UsbPhoneSyncTest : public GuiTest {  // [UsbPhoneSyncTest.md#UsbPhoneSyncTest]
protected:
    void TearDown() override
    {
        if (const auto device = only_device()) {
            Config::instance().erase_by_name(translation::translate("item_name_folder", {wxString::FromUTF8(device->name).ToStdWstring()}));
            Config::instance().remove_device(device->id);
        }
        DeviceLink::instance().stop();
        if (phone_ && phone_->ready())
            phone_->remove_dir("/sdcard" + phone_folder_);
        phone_.reset();
        std::filesystem::remove_all(pc_folder_);
        GuiTest::TearDown();
    }

    std::optional<UsbPhone> phone_;
    const std::string phone_folder_ = unique_phone_folder();
    const std::filesystem::path pc_folder_ = GuiTestSandbox::target_dir() / "handy";
};

DialogAction pair_over_usb(UsbPhone& phone)  // [UsbPhoneSyncTest.md#pair_over_usb]
{
    return [&phone](wxDialog& dialog) {
        PairingOffer offer = DeviceLink::instance().offer();
        offer.addresses = {"127.0.0.1"};
        const bool tapped = phone.reverse(offer.port) && phone.open_link(qr_content(offer)) && phone.tap("connect", PHONE_TIMEOUT);
        EXPECT_TRUE(tapped) << "Kopplung über USB nicht angestoßen: adb reverse, Kopplungs-Link oder \"Verbinden\" in der App.";
        const auto& pairing = dynamic_cast<PairDeviceDialog&>(dialog);
        EXPECT_TRUE(tapped && wait_until([&] { return pairing.paired().has_value(); }, 30000)) << "Der Dialog hat das gekoppelte Handy nicht übernommen.";
        dialog.EndModal(wxID_CLOSE);  // also after a failure, so the test does not wait for the watcher's deadline
    };
}

DialogAction choose_folder_template(const std::filesystem::path& pc_folder, const std::string& phone_folder)
{
    return [=](wxDialog& dialog) {
        ASSERT_TRUE(select_radio_option(find(dialog, ID_TEMPLATES_RADIO), static_cast<int>(PhoneTemplate::Folder)));  // before the folders: the choice resets them
        auto* picker = dynamic_cast<wxDirPickerCtrl*>(find(dialog, ID_TEMPLATES_PC_FOLDER));
        ASSERT_NE(picker, nullptr);
        picker->SetPath(pc_folder.string());
        if (!replace_text(find(dialog, ID_TEMPLATES_PHONE_FOLDER), phone_folder) || picker->GetPath() != wxString(pc_folder.string())) {
            dialog.EndModal(wxID_CANCEL);  // [UsbPhoneSyncTest.md#only-test-folders]
            FAIL() << "Die Testordner ließen sich nicht eintragen.";
        }
        EXPECT_TRUE(click_on(find(dialog, wxID_OK)));
    };
}

}

TEST_F(UsbPhoneSyncTest, OrdnerAufHandyUndPcGleichHalten)  // [UsbPhoneSyncTest.md#folder]
{
    phone_.emplace();
    if (!phone_->ready())
        GTEST_SKIP() << "Kein Handy für den USB-Test: " << phone_->why_not();
    ASSERT_TRUE(phone_->write_file("/sdcard" + phone_folder_ + "/" + FROM_PHONE, "Foto vom Handy")) << "Testdatei auf dem Handy nicht angelegt.";
    std::filesystem::create_directories(pc_folder_);
    std::ofstream(pc_folder_ / FROM_PC) << "Notiz vom PC";
    ModalDialogWatcher::instance().expect(txt::connect_phone(), pair_over_usb(*phone_));
    ModalDialogWatcher::instance().expect(txt::phone_templates(), choose_folder_template(pc_folder_, phone_folder_));

    activate_menu_item(main_window(), tul(id::File_connect_phone));
    pump();

    const auto device = only_device();
    ASSERT_TRUE(device.has_value()) << "Das Handy kam nicht in die Konfiguration.";
    const std::wstring item_name = translation::translate("item_name_folder", {wxString::FromUTF8(device->name).ToStdWstring()});
    const auto item = Config::instance().find_by_name(item_name);
    ASSERT_TRUE(item.has_value()) << "Die Vorlage hat keine Synchronisation angelegt.";
    ASSERT_EQ(item->source(), wxString::FromUTF8(device->id + ":" + phone_folder_)) << "Nur der Testordner des Handys darf synchronisiert werden.";  // [UsbPhoneSyncTest.md#only-test-folders]
    ASSERT_EQ(item->target(), wxString(pc_folder_.string()));
    ASSERT_TRUE(phone_->forward(PHONE_SFTP_PORT)) << "adb forward für den Dateiserver des Handys ging nicht.";
    ASSERT_TRUE(phone_->tap("sync_now", PHONE_TIMEOUT)) << "Die App zeigt \"Jetzt synchronisieren\" nicht.";

    const std::string pc_copy_on_phone = "/sdcard" + phone_folder_ + "/" + FROM_PC;
    EXPECT_TRUE(wait_until([&] { return std::filesystem::exists(pc_folder_ / FROM_PHONE) && phone_->read_file(pc_copy_on_phone); }, PASS_TIMEOUT_MILLISECONDS))
        << "Der Durchlauf hat den Ordner nicht auf beiden Seiten gleich gemacht.";
    EXPECT_EQ(read_text(pc_folder_ / FROM_PHONE), "Foto vom Handy");
    EXPECT_EQ(phone_->read_file(pc_copy_on_phone).value_or(""), "Notiz vom PC");
}

}
