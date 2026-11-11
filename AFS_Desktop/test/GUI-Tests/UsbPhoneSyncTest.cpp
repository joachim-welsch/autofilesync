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
#include "DevicesDialog.h"
#include "PairDeviceDialog.h"
#include "PhoneDirDialog.h"
#include "PhoneTemplatesDialog.h"
#include "SyncItemDialog.h"
#include "id.h"
#include "TextResources.h"
#include "Translation.h"
#include "utilwx.h"
#include <wx/filepicker.h>
#include <wx/listctrl.h>
#include <ctime>
#include <fstream>
#include <random>
#include <set>

using namespace AFS_Core;
using utilwx::tul;

namespace guitest {
namespace {

constexpr uint16_t PHONE_SFTP_PORT = 52382;  // SftpServer.PREFERRED_PORT in the app [UsbPhoneSyncTest.md#sftp-port]
constexpr auto PHONE_TIMEOUT = std::chrono::seconds(20);
constexpr int PASS_TIMEOUT_MILLISECONDS = 120000;
constexpr int MESSAGE_TIMEOUT_MILLISECONDS = 15000;
constexpr int MASS_DELETION_FILES = 12;  // above MASS_DELETION_MIN_FILES [UsbPhoneSyncTest.md#music]

std::string unique_phone_root()  // [UsbPhoneSyncTest.md#phone-folder]
{
    std::mt19937 random(std::random_device{}());
    return "/AFS-GUI-Test-" + std::to_string(random());
}

std::string read_text(const std::filesystem::path& file)
{
    std::ifstream in(file);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_text(const std::filesystem::path& file, const std::string& text)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << text;
}

std::set<std::string> texts_in(const std::filesystem::path& dir)
{
    std::set<std::string> texts;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
        texts.insert(read_text(entry.path()));
    return texts;
}

std::optional<Device> only_device()
{
    const auto devices = Config::instance().devices_snapshot();
    return devices.size() == 1 ? std::optional(devices.front()) : std::nullopt;
}

std::wstring item_name_of(PhoneTemplate which, const Device& device)
{
    const char* key = which == PhoneTemplate::Photos ? "item_name_photos" : which == PhoneTemplate::Music ? "item_name_music" : "item_name_folder";
    return translation::translate(key, {wxString::FromUTF8(device.name).ToStdWstring()});
}

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

DialogAction choose_template(PhoneTemplate which, const std::filesystem::path& pc_folder, const std::string& phone_folder)
{
    return [=](wxDialog& dialog) {
        auto* picker = dynamic_cast<wxDirPickerCtrl*>(find(dialog, ID_TEMPLATES_PC_FOLDER));
        const bool entered = picker && select_radio_option(find(dialog, ID_TEMPLATES_RADIO), static_cast<int>(which))  // before the folders: the choice resets them
            && (picker->SetPath(pc_folder.string()), picker->GetPath() == wxString(pc_folder.string()))
            && replace_text(find(dialog, ID_TEMPLATES_PHONE_FOLDER), phone_folder);
        if (!entered) {
            dialog.EndModal(wxID_CANCEL);  // [UsbPhoneSyncTest.md#only-test-folders]
            FAIL() << "Die Testordner ließen sich nicht eintragen.";
        }
        EXPECT_TRUE(click_on(find(dialog, wxID_OK)));
    };
}

void cancel_dialog(wxDialog& dialog)
{
    dialog.EndModal(wxID_CANCEL);
}

}

class UsbPhoneSyncTest : public GuiTest {  // [UsbPhoneSyncTest.md#UsbPhoneSyncTest]
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        phone_.emplace();
        if (!phone_->ready())
            GTEST_SKIP() << "Kein Handy für den USB-Test: " << phone_->why_not();
    }

    void TearDown() override
    {
        for (const auto& item : Config::instance().syncitems())
            if (!device_id_.empty() && (item.source().starts_with(wxString::FromUTF8(device_id_ + ":").ToStdWstring()) || item.target().starts_with(wxString::FromUTF8(device_id_ + ":").ToStdWstring())))
                Config::instance().erase_by_name(item.name());
        if (!device_id_.empty())
            Config::instance().remove_device(device_id_);
        DeviceLink::instance().stop();
        if (phone_ && phone_->ready())
            phone_->remove_dir("/sdcard" + phone_root_);
        phone_.reset();
        std::filesystem::remove_all(pc_root_);
        GuiTest::TearDown();
    }

    std::string phone_folder(const std::string& name) const { return phone_root_ + "/" + name; }
    std::string on_phone(const std::string& path) const { return "/sdcard" + phone_root_ + "/" + path; }
    std::filesystem::path on_pc(const std::string& path) const { return pc_root_ / path; }

    std::optional<Device> pair(DialogAction templates = cancel_dialog)  // [UsbPhoneSyncTest.md#pair]
    {
        ModalDialogWatcher::instance().expect(txt::connect_phone(), pair_over_usb(*phone_));
        ModalDialogWatcher::instance().expect(txt::phone_templates(), std::move(templates));
        activate_menu_item(main_window(), tul(id::File_connect_phone));
        pump();
        const auto device = only_device();
        if (device)
            device_id_ = device->id;
        return device;
    }

    std::optional<SyncItem> set_up(PhoneTemplate which, const std::string& name)  // [UsbPhoneSyncTest.md#set_up]
    {
        std::filesystem::create_directories(on_pc(name));
        ModalDialogWatcher::instance().expect(txt::devices(), [](wxDialog& dialog) {
            EXPECT_TRUE(click_on(find(dialog, ID_DEVICES_SETUP)));
            dialog.EndModal(wxID_CLOSE);
        });
        ModalDialogWatcher::instance().expect(txt::phone_templates(), choose_template(which, on_pc(name), phone_folder(name)));
        activate_menu_item(main_window(), tul(id::EditDevices));
        pump();
        return item_of(which, name);
    }

    std::optional<SyncItem> item_of(PhoneTemplate which, const std::string& name) const  // [UsbPhoneSyncTest.md#only-test-folders]
    {
        const auto device = only_device();
        const auto item = device ? Config::instance().find_by_name(item_name_of(which, *device)) : std::nullopt;
        if (!item)
            return std::nullopt;
        const std::wstring phone = wxString::FromUTF8(device->id + ":" + phone_folder(name)).ToStdWstring();
        const std::wstring pc = on_pc(name).wstring();
        const bool to_phone = which == PhoneTemplate::Music;
        return item->source() == (to_phone ? pc : phone) && item->target() == (to_phone ? phone : pc) ? item : std::nullopt;
    }

    bool sync()  // [UsbPhoneSyncTest.md#sync]
    {
        if (!forwarded_)
            forwarded_ = phone_->forward(PHONE_SFTP_PORT);
        return forwarded_ && phone_->start_pass() && wait_until([this] { return phone_->pass_finished(); }, PASS_TIMEOUT_MILLISECONDS);
    }

    std::optional<UsbPhone> phone_;
    const std::string phone_root_ = unique_phone_root();
    const std::filesystem::path pc_root_ = GuiTestSandbox::target_dir() / "handy";
    std::string device_id_;
    bool forwarded_ = false;
};

TEST_F(UsbPhoneSyncTest, OrdnerAufHandyUndPcGleichHalten)  // [UsbPhoneSyncTest.md#folder]
{
    ASSERT_TRUE(phone_->write_file(on_phone("ordner/vom-handy.txt"), "Foto vom Handy"));
    write_text(on_pc("ordner/vom-pc.txt"), "Notiz vom PC");

    const auto device = pair(choose_template(PhoneTemplate::Folder, on_pc("ordner"), phone_folder("ordner")));
    ASSERT_TRUE(device.has_value()) << "Das Handy kam nicht in die Konfiguration.";
    ASSERT_TRUE(item_of(PhoneTemplate::Folder, "ordner").has_value()) << "Die Vorlage hat nicht genau die Testordner verbunden.";
    ASSERT_TRUE(sync()) << "Der Durchlauf kam nicht zu Ende.";

    EXPECT_EQ(read_text(on_pc("ordner/vom-handy.txt")), "Foto vom Handy");
    EXPECT_EQ(phone_->read_file(on_phone("ordner/vom-pc.txt")).value_or(""), "Notiz vom PC");
    EXPECT_TRUE(phone_->shows_text(wxString(item_name_of(PhoneTemplate::Folder, *device)).utf8_string())) << "Die App listet die Synchronisation nicht.";
    EXPECT_TRUE(phone_->shows("direction_both"));
    EXPECT_TRUE(phone_->shows("status_done")) << "Die App meldet den Durchlauf nicht als erledigt.";
}

TEST_F(UsbPhoneSyncTest, FotosSichernBehaeltAufDemHandyGeloeschte)  // [UsbPhoneSyncTest.md#photos]
{
    ASSERT_TRUE(phone_->write_file(on_phone("fotos/bild-1.jpg"), "Bild 1"));
    ASSERT_TRUE(phone_->write_file(on_phone("fotos/2026/bild-2.jpg"), "Bild 2"));
    write_text(on_pc("fotos/nur-am-pc.txt"), "bleibt am PC");
    ASSERT_TRUE(pair().has_value());
    ASSERT_TRUE(set_up(PhoneTemplate::Photos, "fotos").has_value()) << "Die Vorlage hat nicht genau die Testordner verbunden.";

    ASSERT_TRUE(sync());
    EXPECT_EQ(read_text(on_pc("fotos/bild-1.jpg")), "Bild 1");
    EXPECT_EQ(read_text(on_pc("fotos/2026/bild-2.jpg")), "Bild 2") << "Unterordner werden mitgesichert.";
    EXPECT_FALSE(phone_->read_file(on_phone("fotos/nur-am-pc.txt"))) << "Die Fotosicherung geht nur vom Handy zum PC.";
    EXPECT_TRUE(phone_->shows("direction_to_pc"));

    ASSERT_TRUE(phone_->remove_file(on_phone("fotos/bild-1.jpg")));
    ASSERT_TRUE(sync());
    EXPECT_EQ(read_text(on_pc("fotos/bild-1.jpg")), "Bild 1") << "Ein auf dem Handy gelöschtes Foto darf auf dem PC nicht verschwinden.";
}

TEST_F(UsbPhoneSyncTest, MusikAufsHandyUebernimmtLoeschungenAberKeineMassenloeschung)  // [UsbPhoneSyncTest.md#music]
{
    std::vector<std::string> songs;
    for (int n = 1; n <= MASS_DELETION_FILES; ++n)
        songs.push_back("lied-" + std::to_string(n + 10) + ".mp3");
    for (const auto& song : songs)
        write_text(on_pc("musik/" + song), song);
    ASSERT_TRUE(phone_->write_file(on_phone("musik/nur-am-handy.mp3"), "nur am Handy"));
    ASSERT_TRUE(pair().has_value());
    ASSERT_TRUE(set_up(PhoneTemplate::Music, "musik").has_value()) << "Die Vorlage hat nicht genau die Testordner verbunden.";

    ASSERT_TRUE(sync());
    EXPECT_EQ(phone_->list_dir(on_phone("musik")), songs) << "Das Handy soll genau die Musik des PCs haben.";
    EXPECT_TRUE(phone_->shows("direction_to_phone"));
    EXPECT_FALSE(std::filesystem::exists(on_pc("musik/nur-am-handy.mp3"))) << "Die Musik geht nur vom PC zum Handy.";

    std::filesystem::remove(on_pc("musik/" + songs.front()));
    songs.erase(songs.begin());
    ASSERT_TRUE(sync());
    EXPECT_EQ(phone_->list_dir(on_phone("musik")), songs) << "Musik, die der PC nicht mehr hat, verschwindet vom Handy.";

    std::filesystem::remove_all(on_pc("musik"));
    std::filesystem::create_directories(on_pc("musik"));
    ASSERT_TRUE(sync());
    EXPECT_EQ(phone_->list_dir(on_phone("musik")), songs) << "Ein leerer Musikordner auf dem PC darf die Musik auf dem Handy nicht löschen.";
    EXPECT_TRUE(phone_->shows("status_done_errors")) << "Die App muss zeigen, dass der Durchlauf nicht alles erledigt hat.";
}

TEST_F(UsbPhoneSyncTest, MassenloeschungWirdVerweigert)  // [UsbPhoneSyncTest.md#mass-deletion]
{
    for (int n = 1; n <= MASS_DELETION_FILES; ++n)
        ASSERT_TRUE(phone_->write_file(on_phone("masse/datei-" + std::to_string(n) + ".txt"), "Datei " + std::to_string(n)));
    ASSERT_TRUE(pair().has_value());
    ASSERT_TRUE(set_up(PhoneTemplate::Folder, "masse").has_value()) << "Die Vorlage hat nicht genau die Testordner verbunden.";
    ASSERT_TRUE(sync());
    ASSERT_EQ(std::distance(std::filesystem::directory_iterator(on_pc("masse")), std::filesystem::directory_iterator()), MASS_DELETION_FILES);

    std::filesystem::remove_all(on_pc("masse"));
    std::filesystem::create_directories(on_pc("masse"));
    ASSERT_TRUE(sync());

    EXPECT_EQ(phone_->list_dir(on_phone("masse")).size(), static_cast<size_t>(MASS_DELETION_FILES)) << "Eine Massenlöschung darf ohne Bestätigung nichts löschen.";
    EXPECT_TRUE(phone_->shows("status_done_errors")) << "Die App muss zeigen, dass der Durchlauf nicht alles erledigt hat.";
}

TEST_F(UsbPhoneSyncTest, ZweiWegeUebernimmtAenderungLoeschungUndUmbenennung)  // [UsbPhoneSyncTest.md#two-way]
{
    ASSERT_TRUE(phone_->write_file(on_phone("zwei/aendern.txt"), "alt"));
    ASSERT_TRUE(phone_->write_file(on_phone("zwei/umbenennen.txt"), "bleibt gleich"));
    write_text(on_pc("zwei/loeschen.txt"), "fliegt raus");
    ASSERT_TRUE(pair().has_value());
    ASSERT_TRUE(set_up(PhoneTemplate::Folder, "zwei").has_value()) << "Die Vorlage hat nicht genau die Testordner verbunden.";
    ASSERT_TRUE(sync());
    ASSERT_TRUE(std::filesystem::exists(on_pc("zwei/umbenennen.txt")));
    ASSERT_TRUE(phone_->read_file(on_phone("zwei/loeschen.txt")));

    ASSERT_TRUE(phone_->write_file(on_phone("zwei/aendern.txt"), "neu und länger"));
    ASSERT_TRUE(phone_->move_file(on_phone("zwei/umbenennen.txt"), on_phone("zwei/umbenannt.txt")));
    std::filesystem::remove(on_pc("zwei/loeschen.txt"));
    ASSERT_TRUE(sync());

    EXPECT_EQ(read_text(on_pc("zwei/aendern.txt")), "neu und länger");
    EXPECT_EQ(read_text(on_pc("zwei/umbenannt.txt")), "bleibt gleich");
    EXPECT_FALSE(std::filesystem::exists(on_pc("zwei/umbenennen.txt"))) << "Die Umbenennung auf dem Handy muss auf dem PC ankommen.";
    EXPECT_FALSE(phone_->read_file(on_phone("zwei/loeschen.txt"))) << "Die Löschung auf dem PC muss auf dem Handy ankommen.";
}

TEST_F(UsbPhoneSyncTest, KonfliktBehaeltBeideFassungen)  // [UsbPhoneSyncTest.md#conflict]
{
    ASSERT_TRUE(phone_->write_file(on_phone("konflikt/notiz.txt"), "Anfang"));
    ASSERT_TRUE(pair().has_value());
    ASSERT_TRUE(set_up(PhoneTemplate::Folder, "konflikt").has_value()) << "Die Vorlage hat nicht genau die Testordner verbunden.";
    ASSERT_TRUE(sync());
    ASSERT_EQ(read_text(on_pc("konflikt/notiz.txt")), "Anfang");

    ASSERT_TRUE(phone_->write_file(on_phone("konflikt/notiz.txt"), "Fassung vom Handy"));
    write_text(on_pc("konflikt/notiz.txt"), "Fassung vom PC, etwas länger");
    ASSERT_TRUE(sync());

    const std::set<std::string> both{"Fassung vom Handy", "Fassung vom PC, etwas länger"};
    EXPECT_EQ(texts_in(on_pc("konflikt")), both) << "Auf dem PC fehlt eine Fassung.";
    std::set<std::string> on_the_phone;
    for (const auto& name : phone_->list_dir(on_phone("konflikt")))
        on_the_phone.insert(phone_->read_file(on_phone("konflikt/" + name)).value_or(""));
    EXPECT_EQ(on_the_phone, both) << "Auf dem Handy fehlt eine Fassung.";
}

TEST_F(UsbPhoneSyncTest, EigeneSynchronisationMitHandyKnopf)  // [UsbPhoneSyncTest.md#own-item]
{
    const wxString name = "USB-Test eigene Synchronisation";
    ASSERT_TRUE(phone_->write_file(on_phone("eigene/bericht.txt"), "vom Handy"));
    std::filesystem::create_directories(on_pc("eigene"));
    const auto device = pair();
    ASSERT_TRUE(device.has_value());
    const std::string folder = phone_folder("eigene");
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), [&](wxDialog& dialog) {
        const bool entered = replace_text(find(dialog, ID_NAME_BOX), name)
            && select_entry(find(dialog, ID_TYPE_CHOICE), sync_item_type_to_str(SyncItemType::Tree))
            && click_on(find(dialog, ID_BTN_PHONE_SOURCE))
            && replace_text(find(dialog, ID_TARGET_DIR_BOX), on_pc("eigene").string());
        EXPECT_TRUE(entered) << "Der Dialog ließ sich nicht ausfüllen.";
        EXPECT_TRUE(entered && click_when_enabled(find(dialog, wxID_OK)));
        if (dialog.IsShown())
            dialog.EndModal(wxID_CANCEL);
    });
    ModalDialogWatcher::instance().expect(txt::phone_dir(), [&](wxDialog& dialog) {
        if (!replace_text(find(dialog, ID_PHONEDIR_FOLDER), folder)) {
            dialog.EndModal(wxID_CANCEL);  // [UsbPhoneSyncTest.md#only-test-folders]
            FAIL() << "Der Handy-Ordner ließ sich nicht eintragen.";
        }
        EXPECT_TRUE(click_on(find(dialog, wxID_OK)));
    });
    activate_menu_item(main_window(), tul(id::File_new_Synchronization));
    pump();

    const auto item = Config::instance().find_by_name(name.ToStdWstring());
    ASSERT_TRUE(item.has_value()) << "Die Synchronisation wurde nicht angelegt.";
    ASSERT_EQ(item->source(), wxString::FromUTF8(device->id + ":" + folder).ToStdWstring()) << "Der Handy-Knopf hat nicht den Testordner eingetragen.";
    ASSERT_EQ(item->target(), on_pc("eigene").wstring());
    ASSERT_TRUE(sync());
    EXPECT_EQ(read_text(on_pc("eigene/bericht.txt")), "vom Handy");
    EXPECT_TRUE(phone_->shows_text(name.utf8_string())) << "Die App listet die eigene Synchronisation nicht.";
}

TEST_F(UsbPhoneSyncTest, GetrenntesHandyWirdAbgewiesen)  // [UsbPhoneSyncTest.md#unpair]
{
    ASSERT_TRUE(phone_->write_file(on_phone("trennen/vorher.txt"), "vorher"));
    const auto device = pair();
    ASSERT_TRUE(device.has_value());
    ASSERT_TRUE(set_up(PhoneTemplate::Folder, "trennen").has_value()) << "Die Vorlage hat nicht genau die Testordner verbunden.";
    ASSERT_TRUE(sync());
    ASSERT_EQ(read_text(on_pc("trennen/vorher.txt")), "vorher");

    ModalDialogWatcher::instance().expect(txt::devices(), [](wxDialog& dialog) {
        auto* list = dynamic_cast<wxListCtrl*>(find(dialog, ID_DEVICES_LIST));
        ASSERT_NE(list, nullptr);
        ASSERT_EQ(list->GetItemCount(), 1);
        EXPECT_NE(list->GetItemText(0, 1), wxString(translation::translate("device_never_seen"))) << "Nach dem Durchlauf muss die letzte Meldung angezeigt werden.";
        EXPECT_TRUE(click_on(*list, 0));
        EXPECT_TRUE(click_when_enabled(find(dialog, ID_DEVICES_UNPAIR)));
        EXPECT_TRUE(wait_until([] { return Config::instance().devices_snapshot().empty(); }));
        dialog.EndModal(wxID_CLOSE);
    });
    ModalDialogWatcher::instance().answer(translation::translate("confirm_delete_title"), wxID_YES);
    activate_menu_item(main_window(), tul(id::EditDevices));
    pump();
    ASSERT_TRUE(Config::instance().devices_snapshot().empty()) << "Das Handy wurde nicht getrennt.";

    ASSERT_TRUE(phone_->write_file(on_phone("trennen/nachher.txt"), "nachher"));
    EXPECT_TRUE(sync());
    EXPECT_TRUE(wait_until([this] { return phone_->shows("status_rejected"); }, MESSAGE_TIMEOUT_MILLISECONDS)) << "Die App sagt nicht, dass der PC das Handy nicht mehr kennt.";
    EXPECT_FALSE(std::filesystem::exists(on_pc("trennen/nachher.txt"))) << "Ein getrenntes Handy darf nichts mehr übertragen.";
}

TEST_F(UsbPhoneSyncTest, CodeGiltNurBeiOffenemDialog)  // [UsbPhoneSyncTest.md#closed-pairing]
{
    PairingOffer offer;
    ModalDialogWatcher::instance().expect(txt::connect_phone(), [&](wxDialog& dialog) {
        offer = DeviceLink::instance().offer();
        offer.addresses = {"127.0.0.1"};
        EXPECT_TRUE(phone_->reverse(offer.port));
        dialog.EndModal(wxID_CLOSE);
    });
    activate_menu_item(main_window(), tul(id::File_connect_phone));
    pump();

    ASSERT_TRUE(phone_->open_link(qr_content(offer)));
    ASSERT_TRUE(phone_->tap("connect", PHONE_TIMEOUT));
    EXPECT_TRUE(wait_until([this] { return phone_->shows("error_pairing_closed") || phone_->shows("error_unreachable"); }, MESSAGE_TIMEOUT_MILLISECONDS))
        << "Die App meldet nicht, dass der PC nicht mehr wartet.";
    EXPECT_TRUE(Config::instance().devices_snapshot().empty()) << "Ein Code nach dem Schließen des Dialogs darf nichts koppeln.";
}

TEST_F(UsbPhoneSyncTest, AbgelaufenerCodeWirdAbgelehnt)  // [UsbPhoneSyncTest.md#expired]
{
    ModalDialogWatcher::instance().expect(txt::connect_phone(), [this](wxDialog& dialog) {
        PairingOffer offer = DeviceLink::instance().offer();
        offer.addresses = {"127.0.0.1"};
        DeviceLink::instance().pairing().end();  // the token runs out while the code is still on the phone
        const bool tapped = phone_->reverse(offer.port) && phone_->open_link(qr_content(offer)) && phone_->tap("connect", PHONE_TIMEOUT);
        EXPECT_TRUE(tapped) << "Kopplung über USB nicht angestoßen: adb reverse, Kopplungs-Link oder \"Verbinden\" in der App.";
        EXPECT_TRUE(tapped && wait_until([this] { return phone_->shows("error_pairing_closed") || phone_->shows("error_token_invalid"); }, MESSAGE_TIMEOUT_MILLISECONDS))
            << "Die App meldet nicht, dass der PC den abgelaufenen Code ablehnt.";
        dialog.EndModal(wxID_CLOSE);
    });
    activate_menu_item(main_window(), tul(id::File_connect_phone));
    pump();

    EXPECT_TRUE(Config::instance().devices_snapshot().empty()) << "Ein abgelaufener Code darf nichts koppeln.";
}

}
