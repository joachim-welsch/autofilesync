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
#include <SmtpMailer.h>
#include <StrUtil.h>
#include <Translation.h>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <sstream>
#ifdef WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
	#include <wincrypt.h>
#else
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <unistd.h>
#endif
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

namespace AFS_Core {

using std::string;
using std::wstring;
using util::wstring_to_string;

namespace {

constexpr uint16_t IMPLICIT_TLS_PORT = 465;
constexpr int SOCKET_TIMEOUT_SECONDS = 15;
constexpr size_t MAX_REPLY_SIZE = 4096;

string base64(const string& data)
{
	if (data.empty())
		return {};
	string encoded((data.size() + 2) / 3 * 4 + 1, '\0');
	int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&encoded[0]),
		reinterpret_cast<const unsigned char*>(data.data()), static_cast<int>(data.size()));
	encoded.resize(written < 0 ? 0 : written);
	return encoded;
}

string base64_lines(const string& data)
{
	constexpr size_t BYTES_PER_LINE = 57;
	string encoded;
	for (size_t pos = 0; pos < data.size(); pos += BYTES_PER_LINE)
		encoded += base64(data.substr(pos, BYTES_PER_LINE)) + "\r\n";
	return encoded;
}

string encoded_word(const wstring& text)
{
	return "=?UTF-8?B?" + base64(wstring_to_string(text)) + "?=";
}

string rfc5322_date()
{
	static const char* const DAYS[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char* const MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	const std::time_t now = std::time(nullptr);
	std::tm utc{};
#ifdef WIN32
	gmtime_s(&utc, &now);
#else
	gmtime_r(&now, &utc);
#endif
	char date[64];
	std::snprintf(date, sizeof date, "%s, %02d %s %04d %02d:%02d:%02d +0000",
		DAYS[utc.tm_wday], utc.tm_mday, MONTHS[utc.tm_mon], utc.tm_year + 1900,
		utc.tm_hour, utc.tm_min, utc.tm_sec);
	return date;
}

string local_host_name()
{
	char name[256];
	if (gethostname(name, sizeof name) != 0)
		return "localhost";
	name[sizeof name - 1] = '\0';
	return name[0] == '\0' ? "localhost" : name;
}

#ifdef WIN32

void load_system_root_certificates(SSL_CTX* ctx)
{
	HCERTSTORE system_store = CertOpenSystemStoreW(0, L"ROOT");
	if (!system_store)
		return;
	X509_STORE* trusted = SSL_CTX_get_cert_store(ctx);
	PCCERT_CONTEXT entry = nullptr;
	while ((entry = CertEnumCertificatesInStore(system_store, entry)) != nullptr) {
		const unsigned char* encoded = entry->pbCertEncoded;
		if (X509* certificate = d2i_X509(nullptr, &encoded, entry->cbCertEncoded)) {
			X509_STORE_add_cert(trusted, certificate);
			X509_free(certificate);
		}
	}
	CertCloseStore(system_store, 0);
}
#endif

class SmtpSession {
public:
	explicit SmtpSession(const SmtpAccount& account)
		: account_(account), host_(wstring_to_string(account.server)) {}
	~SmtpSession();
	SmtpSession(const SmtpSession&) = delete;
	SmtpSession& operator=(const SmtpSession&) = delete;
	string send(const wstring& recipient, const wstring& subject, const string& body);
	bool unencrypted() const { return unencrypted_; }
private:
	bool connect();
	void set_timeouts();
	bool enable_tls();
	bool upgrade_to_tls();
	bool ehlo();
	bool authenticate();
	bool deliver(const wstring& recipient, const wstring& subject, const string& body);
	string message(const wstring& recipient, const wstring& subject, const string& body) const;
	bool put(const string& line);
	bool read_reply(int& code);
	bool reply(int expected);
	bool reply_ok();
	bool fail(const string& reason) { error_ = reason; return false; }

	const SmtpAccount& account_;
	string	 host_;
	BIO*	 bio_ = nullptr;	// the whole chain once TLS is on
	SSL_CTX* ctx_ = nullptr;
	string	 error_;
	string	 last_reply_;
	bool	 unencrypted_ = false;
};

SmtpSession::~SmtpSession()
{
	if (bio_)
		BIO_free_all(bio_);		// frees the TLS BIO and the connection below it
	if (ctx_)
		SSL_CTX_free(ctx_);		// after the BIOs: their SSL objects reference it
}

bool SmtpSession::connect()
{
#ifdef WIN32
	BIO_sock_init();	// OpenSSL does not start Winsock on its own
#endif
	const string address = host_ + ":" + std::to_string(account_.port);
	bio_ = BIO_new_connect(address.c_str());
	if (!bio_)
		return fail(translation::translate_utf8("smtp_connection_not_openable", {address}));
	if (BIO_do_connect(bio_) <= 0)
		return fail(translation::translate_utf8("smtp_no_connection_to") + address);
	set_timeouts();
	return true;
}

void SmtpSession::set_timeouts()
{
	const int socket_handle = BIO_get_fd(bio_, nullptr);
	if (socket_handle < 0)
		return;
#ifdef WIN32
	DWORD timeout = SOCKET_TIMEOUT_SECONDS * 1000;
	const char* value = reinterpret_cast<const char*>(&timeout);
	int size = sizeof timeout;
	SOCKET socket_id = static_cast<SOCKET>(socket_handle);
#else
	timeval timeout{ SOCKET_TIMEOUT_SECONDS, 0 };
	const void* value = &timeout;
	socklen_t size = sizeof timeout;
	int socket_id = socket_handle;
#endif
	setsockopt(socket_id, SOL_SOCKET, SO_RCVTIMEO, value, size);
	setsockopt(socket_id, SOL_SOCKET, SO_SNDTIMEO, value, size);
}

bool SmtpSession::enable_tls()
{
	ctx_ = SSL_CTX_new(TLS_client_method());
	if (!ctx_)
		return fail(translation::translate_utf8("smtp_tls_init_failed"));
	SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
	SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
	SSL_CTX_set_default_verify_paths(ctx_);
#ifdef WIN32
	load_system_root_certificates(ctx_);
#endif
	BIO* tls = BIO_new_ssl(ctx_, 1);
	if (!tls)
		return fail(translation::translate_utf8("smtp_tls_layer_failed"));
	SSL* ssl = nullptr;
	BIO_get_ssl(tls, &ssl);
	if (!ssl) {
		BIO_free(tls);
		return fail(translation::translate_utf8("smtp_tls_layer_failed"));
	}
	SSL_set_tlsext_host_name(ssl, host_.c_str());	// servers with several certificates need SNI
	SSL_set1_host(ssl, host_.c_str());				// makes the handshake check the certificate name
	bio_ = BIO_push(tls, bio_);
	if (BIO_do_handshake(bio_) <= 0)
		return fail(translation::translate_utf8("smtp_tls_handshake_failed", {host_}));
	BIO* buffered = BIO_new(BIO_f_buffer());
	if (!buffered)
		return fail(translation::translate_utf8("smtp_read_buffer_failed", {host_}));
	bio_ = BIO_push(buffered, bio_);
	return true;
}

bool SmtpSession::upgrade_to_tls()
{
	if (!put("STARTTLS"))
		return false;
	int code = 0;
	if (!read_reply(code))
		return false;
	if (code == 220)
		return enable_tls() && ehlo();
	if (!account_.password.empty())
		return fail(translation::translate_utf8("smtp_no_starttls", {host_}) + last_reply_);
	unencrypted_ = true;  // [SmtpMailer.md#plaintext-without-password]
	return true;
}

bool SmtpSession::ehlo()
{
	return put("EHLO " + local_host_name()) && reply_ok();
}

bool SmtpSession::authenticate()
{
	if (account_.user.empty())
		return true;
	if (!put("AUTH LOGIN") || !reply(334))
		return false;
	if (!put(base64(wstring_to_string(account_.user))) || !reply(334))
		return false;
	if (!put(base64(wstring_to_string(account_.password))) || !reply(235))
		return fail(translation::translate_utf8("smtp_login_rejected", {host_}));
	return true;
}

string SmtpSession::message(const wstring& recipient, const wstring& subject, const string& body) const
{
	std::ostringstream os;
	os << "From: <" << wstring_to_string(account_.from) << ">\r\n"
	   << "To: <" << wstring_to_string(recipient) << ">\r\n"
	   << "Subject: " << encoded_word(subject) << "\r\n"
	   << "Date: " << rfc5322_date() << "\r\n"
	   << "MIME-Version: 1.0\r\n"
	   << "Content-Type: text/plain; charset=UTF-8\r\n"
	   << "Content-Transfer-Encoding: base64\r\n"
	   << "\r\n"
	   << base64_lines(body);
	return os.str();
}

bool SmtpSession::deliver(const wstring& recipient, const wstring& subject, const string& body)
{
	if (!put("MAIL FROM:<" + wstring_to_string(account_.from) + ">") || !reply_ok())
		return false;
	if (!put("RCPT TO:<" + wstring_to_string(recipient) + ">") || !reply_ok())
		return false;
	if (!put("DATA") || !reply(354))
		return false;
	if (!put(message(recipient, subject, body) + "\r\n."))
		return false;
	return reply_ok();
}

string SmtpSession::send(const wstring& recipient, const wstring& subject, const string& body)
{
	const bool implicit_tls = account_.port == IMPLICIT_TLS_PORT;
	if (!connect())
		return error_;
	if (implicit_tls && !enable_tls())
		return error_;
	if (!reply(220) || !ehlo())
		return error_;
	if (!implicit_tls && !upgrade_to_tls())
		return error_;
	if (!authenticate() || !deliver(recipient, subject, body))
		return error_;
	put("QUIT");	// the server has accepted the mail, its answer changes nothing
	return {};
}

bool SmtpSession::put(const string& line)
{
	const string data = line + "\r\n";
	size_t written = 0;
	while (written < data.size()) {
		const int chunk = BIO_write(bio_, data.data() + written, static_cast<int>(data.size() - written));
		if (chunk <= 0)
			return fail(translation::translate_utf8("smtp_send_failed", {host_}));
		written += chunk;
	}
	BIO_flush(bio_);
	return true;
}

bool SmtpSession::read_reply(int& code)
{
	char buffer[512];
	for (;;) {
		int length = BIO_gets(bio_, buffer, sizeof buffer);
		if (length <= 0)
			return fail(translation::translate_utf8("smtp_no_answer_from") + host_);
		string line(buffer, length);
		while (line.back() != '\n' && line.size() < MAX_REPLY_SIZE) {	// reply longer than the buffer
			length = BIO_gets(bio_, buffer, sizeof buffer);
			if (length <= 0)
				return fail(translation::translate_utf8("smtp_truncated_answer_from") + host_);
			line.append(buffer, length);
		}
		while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			line.pop_back();
		if (line.size() < 3 || !std::isdigit(static_cast<unsigned char>(line[0]))
			|| !std::isdigit(static_cast<unsigned char>(line[1])) || !std::isdigit(static_cast<unsigned char>(line[2])))
			return fail(translation::translate_utf8("smtp_unexpected_answer", {host_}) + line);
		code = std::stoi(line.substr(0, 3));
		last_reply_ = line;
		if (line.size() == 3 || line[3] != '-')	// '-' marks a continuation line
			return true;
	}
}

bool SmtpSession::reply(int expected)
{
	int code = 0;
	if (!read_reply(code))
		return false;
	return code == expected ? true : fail(translation::translate_utf8("smtp_server_answers", {host_}) + last_reply_);
}

bool SmtpSession::reply_ok()
{
	int code = 0;
	if (!read_reply(code))
		return false;
	return code >= 200 && code < 300 ? true : fail(translation::translate_utf8("smtp_server_answers", {host_}) + last_reply_);
}

} // namespace

MailResult send_mail(const SmtpAccount& account, const std::wstring& recipient,
	const std::wstring& subject, const std::string& body)
{
	if (!account.complete())
		return {translation::translate_utf8("smtp_not_configured")};
	if (recipient.empty())
		return {translation::translate_utf8("smtp_no_recipient")};
	try {
		SmtpSession session(account);
		const string error = session.send(recipient, subject, body);
		return {error, error.empty() && session.unencrypted()};
	}
	catch (const std::exception& ex) {
		return {ex.what()};
	}
}

} // namespace AFS_Core
