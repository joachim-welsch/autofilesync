/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include "pch.h"
#ifdef _UNIX
#include "FileUtil.h"
#include "Logging.h"
#include "ScopedEnv.h"
#include "ScopedLog.h"
#include "ScopedLoggingSetup.h"
#include "SmtpMailer.h"
#include "SmtpSink.h"
#include "TestPaths.h"
#include "Translation.h"
#include "Util.h"
#include <algorithm>
#include <clocale>
#include <chrono>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace AFS_Core;
using namespace std::chrono_literals;

namespace {

namespace fs = std::filesystem;

bool received(const tu::SmtpSink& sink, const std::string& command_prefix)
{
    const auto commands = sink.commands();
    return std::any_of(commands.begin(), commands.end(), [&](const std::string& c) { return c.rfind(command_prefix, 0) == 0; });
}

uint16_t port_nobody_listens_on()  // [SmtpMailerTests.md#port_nobody_listens_on]
{
    const int probe = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(probe, reinterpret_cast<sockaddr*>(&address), sizeof address);
    socklen_t size = sizeof address;
    getsockname(probe, reinterpret_cast<sockaddr*>(&address), &size);
    close(probe);
    return ntohs(address.sin_port);
}

std::string unique(const std::string& text)  // [SmtpMailerTests.md#unique]
{
    return text + " #" + std::to_string(TestPaths::process_id()) + "/" + ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

} // namespace

TEST(SmtpMailerTests, IncompleteAccountIsRefusedBeforeAnyConnection)  // [SmtpMailerTests.md#IncompleteAccountIsRefusedBeforeAnyConnection]
{
    tu::SmtpSink sink;
    SmtpAccount account = sink.account();
    account.server.clear();

    EXPECT_EQ(send_mail(account, L"admin@example.org", L"Betreff", "Text").error, translation::translate_utf8("smtp_not_configured"));

    EXPECT_FALSE(sink.wait_for_connections(1, 100ms));
}

TEST(SmtpMailerTests, EmptyRecipientIsRefusedBeforeAnyConnection)
{
    tu::SmtpSink sink;

    EXPECT_EQ(send_mail(sink.account(), L"", L"Betreff", "Text").error, translation::translate_utf8("smtp_no_recipient"));

    EXPECT_FALSE(sink.wait_for_connections(1, 100ms));
}

TEST(SmtpMailerTests, UnreachableServerIsReportedNotThrown)
{
    SmtpAccount account;
    account.server = L"127.0.0.1";
    account.port = port_nobody_listens_on();
    account.from = L"afs@example.org";

    const std::string error = send_mail(account, L"admin@example.org", L"Betreff", "Text").error;

    EXPECT_NE(error.find("127.0.0.1"), std::string::npos) << error;
}

TEST(SmtpMailerTests, MailReachesAPlainServerWithoutPassword)  // [SmtpMailerTests.md#MailReachesAPlainServerWithoutPassword]
{
    tu::SmtpSink sink;
    const std::string body = "Erste Zeile\nZweite Zeile mit Umlauten: äöü ß\n";

    const MailResult result = send_mail(sink.account(), L"admin@example.org", L"Fehler im Größenvergleich", body);

    EXPECT_EQ(result.error, "");
    EXPECT_TRUE(result.unencrypted) << "the caller must learn that the mail went over a plain connection";

    ASSERT_TRUE(sink.wait_for_connections(1, 2s));
    const auto mails = sink.mails();
    ASSERT_EQ(mails.size(), 1u);
    EXPECT_EQ(mails[0].from, "afs@example.org");
    EXPECT_EQ(mails[0].to, "admin@example.org");
    EXPECT_EQ(mails[0].subject(), "Fehler im Größenvergleich");
    EXPECT_EQ(mails[0].body(), body);
    EXPECT_NE(mails[0].data.find("Content-Type: text/plain; charset=UTF-8"), std::string::npos) << mails[0].data;
    EXPECT_TRUE(received(sink, "STARTTLS")) << "der Client muss TLS zuerst anbieten";
    EXPECT_TRUE(received(sink, "QUIT"));
}

TEST(SmtpMailerTests, PasswordIsNeverSentWithoutTls)  // [SmtpMailerTests.md#PasswordIsNeverSentWithoutTls]
{
    tu::SmtpSink sink;
    SmtpAccount account = sink.account();
    account.user = L"afs";
    account.password = L"testonly-smtp-secret";

    const std::string error = send_mail(account, L"admin@example.org", L"Betreff", "Text").error;

    EXPECT_FALSE(error.empty());
    ASSERT_TRUE(sink.wait_for_connections(1, 2s));
    EXPECT_FALSE(received(sink, "AUTH")) << "Zugangsdaten gingen ohne TLS ueber die Leitung";
    EXPECT_FALSE(received(sink, "MAIL FROM")) << "ohne Anmeldung darf keine Mail versucht werden";
    EXPECT_TRUE(sink.mails().empty());
}

TEST(SmtpMailerTests, RejectedRecipientIsReportedWithTheServersAnswer)
{
    tu::SmtpSink sink;
    sink.reply_to_rcpt("550 no such user here");

    const std::string error = send_mail(sink.account(), L"niemand@example.org", L"Betreff", "Text").error;

    EXPECT_NE(error.find("550 no such user here"), std::string::npos) << error;
    ASSERT_TRUE(sink.wait_for_connections(1, 2s));
    EXPECT_TRUE(sink.mails().empty());
    EXPECT_FALSE(received(sink, "DATA"));
}

namespace {

class SmtpTlsTest : public ::testing::Test {  // [SmtpMailerTests.md#SmtpTlsTest]
protected:
    const fs::path certificate_ = fs::temp_directory_path() / ("autofilesync_smtp_" + std::to_string(TestPaths::process_id()) + ".pem");
    tu::SmtpSink sink_{tu::SmtpSink::Security::StartTls};
    const tu::SmtpSink::Login login_{"afs", "testonly-smtp-secret"};

    void SetUp() override
    {
        tu::write_text_file(certificate_, sink_.certificate_pem());
        sink_.accept_login(login_);
    }

    void TearDown() override { fs::remove(certificate_); }

    SmtpAccount account(const std::string& password) const
    {
        SmtpAccount account = sink_.account();
        account.user = util::string_to_wstring(login_.user);
        account.password = util::string_to_wstring(password);
        return account;
    }
};

} // namespace

TEST_F(SmtpTlsTest, MailWithLoginGoesThroughTls)  // [SmtpMailerTests.md#MailWithLoginGoesThroughTls]
{
    const ScopedEnv trusted("SSL_CERT_FILE", certificate_.c_str());

    const MailResult result = send_mail(account(login_.password), L"admin@example.org", L"Betreff", "verschluesselt");

    EXPECT_EQ(result.error, "");
    EXPECT_FALSE(result.unencrypted);
    ASSERT_TRUE(sink_.wait_for_connections(1, 2s));
    ASSERT_EQ(sink_.logins().size(), 1u);
    EXPECT_EQ(sink_.logins().front(), login_) << "Benutzer und Passwort muessen unveraendert ankommen";
    ASSERT_EQ(sink_.mails().size(), 1u);
    EXPECT_EQ(sink_.mails().front().body(), "verschluesselt");
    const auto commands = sink_.commands();
    const auto position = [&](const std::string& command) { return std::find(commands.begin(), commands.end(), command); };
    EXPECT_LT(position("STARTTLS"), position("AUTH LOGIN")) << "die Anmeldung darf erst nach dem TLS-Handshake kommen";
}

TEST_F(SmtpTlsTest, UntrustedCertificateGetsNeitherPasswordNorMail)  // [SmtpMailerTests.md#UntrustedCertificateGetsNeitherPasswordNorMail]
{
    const ScopedEnv system_store_only("SSL_CERT_FILE", nullptr);

    const MailResult result = send_mail(account(login_.password), L"admin@example.org", L"Betreff", "Text");

    EXPECT_FALSE(result.error.empty());
    ASSERT_TRUE(sink_.wait_for_connections(1, 2s));
    EXPECT_TRUE(sink_.logins().empty()) << "einem Server mit unbekanntem Zertifikat darf kein Passwort geschickt werden";
    EXPECT_TRUE(sink_.mails().empty());
}

TEST_F(SmtpTlsTest, RejectedLoginIsReportedAndSendsNoMail)  // [SmtpMailerTests.md#RejectedLoginIsReportedAndSendsNoMail]
{
    const ScopedEnv trusted("SSL_CERT_FILE", certificate_.c_str());

    const MailResult result = send_mail(account("falsch"), L"admin@example.org", L"Betreff", "Text");

    EXPECT_NE(result.error.find(translation::translate_utf8("smtp_login_rejected", {"127.0.0.1"})), std::string::npos) << result.error;
    ASSERT_TRUE(sink_.wait_for_connections(1, 2s));
    EXPECT_FALSE(received(sink_, "MAIL FROM"));
    EXPECT_TRUE(sink_.mails().empty());
}

namespace {

class FatalMailTest : public ::testing::Test {  // [SmtpMailerTests.md#FatalMailTest]
protected:
    const tu::ScopedLoggingSetup restore_;
    const tu::ScopedLog log_{fs::temp_directory_path() / ("autofilesync_fatalmail_" + std::to_string(TestPaths::process_id()) + ".log"), LogLevel::Info};
    tu::SmtpSink sink_;
    const std::string saved_locale_ = std::setlocale(LC_ALL, nullptr);
    bool catalogue_usable_ = false;

    void SetUp() override
    {
        if (!translation::locale_dir().empty()) {  // [SmtpMailerTests.md#catalogue]
            translation::init_locale(translation::DEFAULT_LOCALES);
            translation::init(translation::locale_dir());
            translation::set_language("en");
            catalogue_usable_ = std::string(std::setlocale(LC_MESSAGES, nullptr)) != "C";
        }
        Logging& log = Logging::instance();
        log.email().smtp_account(sink_.account());
        log.email().notify_address(L"admin@example.org");
        log.email().notify_active(true);
    }

    void TearDown() override
    {
        translation::set_language("");
        std::setlocale(LC_ALL, saved_locale_.c_str());
    }

    std::string log_text() const
    {
        std::ifstream in(log_.path());
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
};

} // namespace

TEST_F(FatalMailTest, FatalErrorIsMailedToTheConfiguredAddress)  // [SmtpMailerTests.md#FatalErrorIsMailedToTheConfiguredAddress]
{
    const std::string message = unique("Fataler Testfehler");

    log_fatal(message);

    ASSERT_TRUE(sink_.wait_for_connections(1, 2s));
    const auto mails = sink_.mails();
    ASSERT_EQ(mails.size(), 1u);
    EXPECT_EQ(mails[0].to, "admin@example.org");
    EXPECT_EQ(mails[0].from, "afs@example.org");
    EXPECT_EQ(mails[0].subject(), translation::translate_utf8("fatal_error_mail_subject"));
    EXPECT_NE(mails[0].body().find(message), std::string::npos) << mails[0].body();
}

TEST_F(FatalMailTest, TheSameFatalErrorIsMailedOnlyOnce)  // [SmtpMailerTests.md#TheSameFatalErrorIsMailedOnlyOnce]
{
    const std::string message = unique("Wiederholter Testfehler");
    log_fatal(message);
    ASSERT_TRUE(sink_.wait_for_connections(1, 2s));

    log_fatal(message);
    log_fatal(unique("Anderer Testfehler"));

    ASSERT_TRUE(sink_.wait_for_connections(2, 2s));
    EXPECT_FALSE(sink_.wait_for_connections(3, 200ms)) << "die Wiederholung desselben Fehlers wurde erneut verschickt";
    const auto mails = sink_.mails();
    ASSERT_EQ(mails.size(), 2u);
    EXPECT_NE(mails[1].body().find("Anderer Testfehler"), std::string::npos);
}

TEST_F(FatalMailTest, ErrorsBelowFatalAreNotMailed)
{
    log_error(unique("Nur ein Fehler"));
    log_warning(unique("Nur eine Warnung"));

    EXPECT_FALSE(sink_.wait_for_connections(1, 200ms));
}

TEST_F(FatalMailTest, NothingIsMailedWhileTheNotificationIsOff)  // [SmtpMailerTests.md#nothing-without-consent]
{
    Logging::instance().email().notify_active(false);

    log_fatal(unique("Stiller Testfehler"));

    EXPECT_FALSE(sink_.wait_for_connections(1, 200ms));
}

TEST_F(FatalMailTest, NothingIsMailedWithoutAnAddress)  // [SmtpMailerTests.md#nothing-without-consent]
{
    Logging::instance().email().notify_address(L"");

    log_fatal(unique("Adressloser Testfehler"));

    EXPECT_FALSE(sink_.wait_for_connections(1, 200ms));
}

TEST_F(FatalMailTest, NothingIsMailedWithAnIncompleteAccount)  // [SmtpMailerTests.md#nothing-without-consent]
{
    SmtpAccount account = sink_.account();
    account.from.clear();
    Logging::instance().email().smtp_account(account);

    log_fatal(unique("Absenderloser Testfehler"));

    EXPECT_FALSE(sink_.wait_for_connections(1, 200ms));
}

TEST_F(FatalMailTest, FailedDeliveryIsLoggedAsAnError)  // [SmtpMailerTests.md#FailedDeliveryIsLoggedAsAnError]
{
    sink_.reply_to_rcpt("550 mailbox unavailable");

    log_fatal(unique("Unzustellbarer Testfehler"));

    ASSERT_TRUE(sink_.wait_for_connections(1, 2s));
    EXPECT_TRUE(sink_.mails().empty());
    const std::string log = log_text();
    EXPECT_NE(log.find("550 mailbox unavailable"), std::string::npos) << log;
    if (catalogue_usable_) {  // braces: the EXPECT macro expands to an if/else of its own
        EXPECT_NE(log.find("admin@example.org"), std::string::npos) << log;
    }
}
#endif
