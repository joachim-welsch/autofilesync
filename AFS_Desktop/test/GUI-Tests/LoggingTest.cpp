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
#include "id.h"
#include "utilwx.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace AFS_Core;

namespace fs = std::filesystem;
using utilwx::tul;

namespace guitest {
namespace {

const wxString LOGGING_TITLE = "Logging-Einstellungen";

void applyLogging(MainWindow& window, const LoggingInput& input, const wxString& expectedMessageBox = wxString())  // [LoggingTest.md#applyLogging]
{
    ModalDialogWatcher::instance().expect(LOGGING_TITLE, fillLoggingDialog(input));
    if (!expectedMessageBox.empty())
        ModalDialogWatcher::instance().answer(expectedMessageBox, wxID_OK);
    activateMenuItem(window, tul(id::EditLogging));
    pump();
}

class RestoredLogging {  // [LoggingTest.md#RestoredLogging]
public:
    RestoredLogging()
        : path_(Logging::instance().get_log_file_path_name()), level_(Logging::instance().get_log_level())
        , retentionDays_(Logging::instance().retention_days()), editor_(Logging::instance().get_editor_path_name())
        , emailActive_(Logging::instance().notify_email_active()), email_(Logging::instance().notify_email())
        , smtp_(Logging::instance().smtp_account()), backupProofPath_(Config::instance().settings().backup_proof_path()) {}
    ~RestoredLogging()
    {
        Logging& logging = Logging::instance();
        logging.set_log_file_path_name(path_);
        logging.set_log_level(level_);
        logging.retention_days(retentionDays_);
        logging.set_editor_path_name(editor_);
        logging.notify_email_active(emailActive_);
        logging.notify_email(email_);
        logging.smtp_account(smtp_);
        Config::instance().settings().backup_proof_path(backupProofPath_);
    }
private:
    std::wstring path_;
    LogLevel level_;
    uint16_t retentionDays_;
    std::wstring editor_;
    bool emailActive_;
    std::wstring email_;
    SmtpAccount smtp_;
    std::wstring backupProofPath_;
};

std::string logLineDaysAgo(int days, const std::string& text)  // [LoggingTest.md#logLineDaysAgo]
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

std::string readFile(const fs::path& pathname)
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
    input.logPath = tree.path("neu.log").string();
    input.logLevel = "Debug";
    input.editor = "notepad";

    applyLogging(mainWindow(), input);

    EXPECT_EQ(Logging::instance().get_log_file_path_name(), tree.path("neu.log").wstring());
    EXPECT_EQ(Logging::instance().get_log_level(), LogLevel::Debug);
    EXPECT_EQ(Logging::instance().get_editor_path_name(), L"notepad");
    EXPECT_TRUE(Config::instance().changed()) << "Die geänderten Logging-Einstellungen gelten nicht als zu speichern.";
}

TEST_F(GuiTest, KuerzereHistorieKuerztLogSofort)  // [LoggingTest.md#KuerzereHistorieKuerztLogSofort]
{
    const RestoredLogging restored;
    const SandboxTree tree(GuiTestSandbox::root() / "historie");
    std::ofstream(tree.path("alt.log")) << logLineDaysAgo(10, "zehn Tage alt") << '\n' << logLineDaysAgo(0, "von heute") << '\n';
    LoggingInput toNewLog;
    toNewLog.logPath = tree.path("alt.log").string();
    toNewLog.retentionDays = 30;
    applyLogging(mainWindow(), toNewLog);
    ASSERT_EQ(Logging::instance().retention_days(), 30);
    ASSERT_NE(readFile(tree.path("alt.log")).find("zehn Tage alt"), std::string::npos);

    LoggingInput shorter;
    shorter.retentionDays = 5;
    applyLogging(mainWindow(), shorter);

    const std::string log = readFile(tree.path("alt.log"));
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

    applyLogging(mainWindow(), input, _("E-Mail"));

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Die fehlende Adresse wurde ohne Meldung angenommen.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answeredMessages().front().Contains("E-Mail-Adresse"));
    EXPECT_FALSE(Logging::instance().notify_email_active()) << "Die Benachrichtigung wurde trotz Ablehnung aktiviert.";
}

TEST_F(GuiTest, EmailAktivOhneServerOderAbsenderWirdAbgelehnt)  // [LoggingTest.md#email-validation]
{
    const RestoredLogging restored;
    LoggingInput input;
    input.email.active = true;
    input.email.address = "wer@example.org";
    input.email.server = "";
    input.email.sender = "afs@example.org";

    applyLogging(mainWindow(), input, _("E-Mail"));

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der fehlende Server wurde ohne Meldung angenommen.";
    EXPECT_TRUE(ModalDialogWatcher::instance().answeredMessages().front().Contains("Postausgangsserver"));
    EXPECT_FALSE(Logging::instance().notify_email_active());
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

    applyLogging(mainWindow(), input);

    EXPECT_TRUE(Logging::instance().notify_email_active());
    EXPECT_EQ(Logging::instance().notify_email(), L"wer@example.org");
    EXPECT_EQ(Logging::instance().smtp_account().server, L"mail.example.org");
    EXPECT_EQ(Logging::instance().smtp_account().port, 465);
    EXPECT_EQ(Logging::instance().smtp_account().from, L"afs@example.org");
}

TEST_F(GuiTest, BackupNachweisWirdNachLaufGeschrieben)  // [LoggingTest.md#BackupNachweisWirdNachLaufGeschrieben]
{
    const RestoredLogging restored;
    const SandboxTree tree(GuiTestSandbox::root() / "nachweis", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}, {"nachweise/", ""}});
    LoggingInput logging;
    logging.backupProofPath = tree.path("nachweise").string();
    applyLogging(mainWindow(), logging);
    ASSERT_EQ(Config::instance().settings().backup_proof_path(), tree.path("nachweise").wstring());
    SyncItemInput input{"guitest-nachweis", tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    input.options.backupProof = true;
    ASSERT_TRUE(addSyncItem(mainWindow(), input));

    ASSERT_TRUE(syncOnce(mainWindow()));

    const fs::path proof = copy_log_file_path(tree.path("nachweise"), L"guitest-nachweis");
    ASSERT_TRUE(fs::exists(proof)) << "Die Nachweisdatei " << proof << " wurde nicht geschrieben.";
    const std::string content = readFile(proof);
    EXPECT_NE(content.find("Erfolgreich"), std::string::npos) << "Der Nachweis meldet keinen Erfolg:\n" << content;
    EXPECT_NE(content.find("guitest-nachweis"), std::string::npos) << "Der Nachweis nennt die Synchronisation nicht:" << std::endl << content;
}

TEST_F(GuiTest, LogDateiAnzeigenOhneEditorZeigtFehler)
{
    const RestoredLogging restored;
    LoggingInput input;
    input.editor = (GuiTestSandbox::root() / "gibt-es-nicht.exe").string();
    applyLogging(mainWindow(), input);
    ModalDialogWatcher::instance().answer("Fehler", wxID_OK);

    activateMenuItem(mainWindow(), tul(id::View_Log));
    pump();

    ASSERT_EQ(ModalDialogWatcher::instance().answeredMessages().size(), 1u) << "Der fehlende Editor blieb ohne Meldung.";
    EXPECT_EQ(ModalDialogWatcher::instance().answeredMessages().front(), "Der konfigurierte Editor konnte nicht gestartet werden.");
}

}
