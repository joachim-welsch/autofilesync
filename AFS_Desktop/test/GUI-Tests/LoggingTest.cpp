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
#include "LoggingDialogDriver.h"
#include "ModalDialogWatcher.h"
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialogDriver.h"
#include "UserActions.h"
#include "Config.h"
#include "CopyLog.h"
#include "Logging.h"
#include "TextResources.h"
#include "Translation.h"
#include "Util.h"
#include "id.h"
#include "utilwx.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

std::wstring proven_status(const fs::path& proof)  // [LoggingTest.md#proven_status]
{
    boost::property_tree::wptree root;
    std::wistringstream text(util::read_utf8_file(proof));
    boost::property_tree::read_json(text, root);
    const auto& entry = root.front().second.back().second;
    const auto found = entry.find(translation::translate("backup_proof_field_status"));
    return found == entry.not_found() ? std::wstring() : found->second.data();
}

wxString logging_title()  // translated at the call, once the catalogue is bound
{
    return txt::logging_settings();
}

void apply_logging(MainWindow& window, const LoggingInput& input, const wxString& expected_message_box = wxString())  // [LoggingTest.md#apply_logging]
{
    ModalDialogWatcher::instance().expect(logging_title(), fill_logging_dialog(input));
    if (!expected_message_box.empty())
        ModalDialogWatcher::instance().answer(expected_message_box, wxID_OK);
    activate_menu_item(window, tul(id::EditLogging));
    pump();
}

class RestoredLogging {  // [LoggingTest.md#RestoredLogging]
public:
    RestoredLogging()
        : path_(Logging::instance().get_log_file_path_name()), level_(Logging::instance().get_log_level())
        , retention_days_(Logging::instance().retention_days()), editor_(Logging::instance().get_editor_path_name())
        , email_active_(Logging::instance().email().notify_active()), email_(Logging::instance().email().notify_address())
        , smtp_(Logging::instance().email().smtp_account()), backup_proof_path_(Config::instance().settings().backup_proof_path()) {}
    ~RestoredLogging()
    {
        Logging& logging = Logging::instance();
        logging.set_log_file_path_name(path_);
        logging.set_log_level(level_);
        logging.retention_days(retention_days_);
        logging.set_editor_path_name(editor_);
        logging.email().notify_active(email_active_);
        logging.email().notify_address(email_);
        logging.email().smtp_account(smtp_);
        Config::instance().settings().backup_proof_path(backup_proof_path_);
    }
private:
    std::wstring path_;
    LogLevel level_;
    uint16_t retention_days_;
    std::wstring editor_;
    bool email_active_;
    std::wstring email_;
    SmtpAccount smtp_;
    std::wstring backup_proof_path_;
};

std::string log_line_days_ago(int days, const std::string& text)  // [LoggingTest.md#log_line_days_ago]
{
    const std::time_t then = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() - std::chrono::hours(24 * days));
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &then);
#else
    localtime_r(&then, &local);
#endif
    std::ostringstream line;
    line << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << ".000 G Info\t" << text;
    return line.str();
}

std::string read_file(const fs::path& pathname)
{
    std::ostringstream content;
    content << std::ifstream(pathname).rdbuf();
    return content.str();
}

}

TEST_F(GuiTest, LogPfadUndLevelWerdenUebernommen)
{
    const RestoredLogging restored;
    const SandboxTree tree(GuiTestSandbox::root() / "logging");
    LoggingInput input;
    input.log_path = tree.path("neu.log").string();
    input.log_level = "Debug";
    input.editor = "notepad";

    apply_logging(main_window(), input);

    EXPECT_EQ(Logging::instance().get_log_file_path_name(), tree.path("neu.log").wstring());
    EXPECT_EQ(Logging::instance().get_log_level(), LogLevel::Debug);
    EXPECT_EQ(Logging::instance().get_editor_path_name(), L"notepad");
    EXPECT_FALSE(Config::instance().changed()) << "Die geänderten Logging-Einstellungen wurden nicht sofort gespeichert.";
    EXPECT_NE(read_file(GuiTestSandbox::config_file()).find("neu.log"), std::string::npos) << "Der neue Logpfad steht nicht in der Datei.";
}

TEST_F(GuiTest, KuerzereHistorieKuerztLogSofort)  // [LoggingTest.md#KuerzereHistorieKuerztLogSofort]
{
    const RestoredLogging restored;
    const SandboxTree tree(GuiTestSandbox::root() / "historie");
    std::ofstream(tree.path("alt.log")) << log_line_days_ago(10, "zehn Tage alt") << '\n' << log_line_days_ago(0, "von heute") << '\n';
    LoggingInput to_new_log;
    to_new_log.log_path = tree.path("alt.log").string();
    to_new_log.retention_days = 30;
    apply_logging(main_window(), to_new_log);
    ASSERT_EQ(Logging::instance().retention_days(), 30);
    ASSERT_NE(read_file(tree.path("alt.log")).find("zehn Tage alt"), std::string::npos);

    LoggingInput shorter;
    shorter.retention_days = 5;
    apply_logging(main_window(), shorter);

    const std::string log = read_file(tree.path("alt.log"));
    EXPECT_EQ(log.find("zehn Tage alt"), std::string::npos) << "Die alte Zeile überlebte die kürzere Historie:\n" << log;
    EXPECT_NE(log.find("von heute"), std::string::npos) << "Die junge Zeile wurde mit gelöscht:\n" << log;
}

TEST_F(GuiTest, EmailAktivOhneAdresseWirdAbgelehnt)  // [LoggingTest.md#email-validation]
{
    const RestoredLogging restored;
    LoggingInput input;
    input.email.active = true;
    input.email.address = "";
    input.email.server = "mail.example.org";
    input.email.sender = "afs@example.org";

    apply_logging(main_window(), input, translation::translate("validation_error"));

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Die fehlende Adresse wurde ohne Meldung angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().front(), translation::translate("email_address_required"));
    EXPECT_FALSE(Logging::instance().email().notify_active()) << "Die Benachrichtigung wurde trotz Ablehnung aktiviert.";
}

TEST_F(GuiTest, EmailAktivOhneServerOderAbsenderWirdAbgelehnt)  // [LoggingTest.md#email-validation]
{
    const RestoredLogging restored;
    LoggingInput input;
    input.email.active = true;
    input.email.address = "wer@example.org";
    input.email.server = "";
    input.email.sender = "afs@example.org";

    apply_logging(main_window(), input, translation::translate("validation_error"));

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Der fehlende Server wurde ohne Meldung angenommen.";
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().front(), translation::translate("smtp_server_required"));
    EXPECT_FALSE(Logging::instance().email().notify_active());
}

TEST_F(GuiTest, EmailMitAllenAngabenWirdUebernommen)
{
    const RestoredLogging restored;
    LoggingInput input;
    input.email.active = true;
    input.email.address = "wer@example.org";
    input.email.server = "mail.example.org";
    input.email.port = 465;
    input.email.user = "benutzer";
    input.email.password = "geheim";
    input.email.sender = "afs@example.org";

    apply_logging(main_window(), input);

    EXPECT_TRUE(Logging::instance().email().notify_active());
    EXPECT_EQ(Logging::instance().email().notify_address(), L"wer@example.org");
    EXPECT_EQ(Logging::instance().email().smtp_account().server, L"mail.example.org");
    EXPECT_EQ(Logging::instance().email().smtp_account().port, 465);
    EXPECT_EQ(Logging::instance().email().smtp_account().from, L"afs@example.org");
}

TEST_F(GuiTest, BackupNachweisWirdNachLaufGeschrieben)  // [LoggingTest.md#BackupNachweisWirdNachLaufGeschrieben]
{
    const RestoredLogging restored;
    const SandboxTree tree(GuiTestSandbox::root() / "nachweis", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}, {"nachweise/", ""}});
    LoggingInput logging;
    logging.backup_proof_path = tree.path("nachweise").string();
    apply_logging(main_window(), logging);
    ASSERT_EQ(Config::instance().settings().backup_proof_path(), tree.path("nachweise").wstring());
    SyncItemInput input{"guitest-nachweis", tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    input.options.backup_proof = true;
    ASSERT_TRUE(add_sync_item(main_window(), input));

    ASSERT_TRUE(sync_once(main_window()));

    const fs::path proof = copy_log_file_path(tree.path("nachweise"), L"guitest-nachweis");
    ASSERT_TRUE(fs::exists(proof)) << "Die Nachweisdatei " << proof << " wurde nicht geschrieben.";
    const std::string content = read_file(proof);
    EXPECT_EQ(proven_status(proof), translation::translate("backup_proof_status_successful")) << "Der Nachweis meldet keinen Erfolg:\n" << content;
    EXPECT_NE(content.find("guitest-nachweis"), std::string::npos) << "Der Nachweis nennt die Synchronisation nicht:" << std::endl << content;
}

TEST_F(GuiTest, LogDateiAnzeigenOhneEditorZeigtFehler)
{
    const RestoredLogging restored;
    LoggingInput input;
    input.editor = (GuiTestSandbox::root() / "gibt-es-nicht.exe").string();
    apply_logging(main_window(), input);
    ModalDialogWatcher::instance().answer(translation::translate("error"), wxID_OK);

    activate_menu_item(main_window(), tul(id::View_Log));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answered_messages().size(), 1u) << "Der fehlende Editor blieb ohne Meldung.";
    EXPECT_EQ(ModalDialogWatcher::instance().answered_messages().front(), translation::translate("editor_start_failed"));
}

}
