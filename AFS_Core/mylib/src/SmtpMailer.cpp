#include "pch.h"
#include <SmtpMailer.h>
#include <strutil.h>
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

using std::string;
using std::wstring;
using util::wstring_to_string;

namespace {

// Only the port distinguishes the two ways to get TLS: 465 expects the
// handshake right after connecting, every other port the STARTTLS command.
constexpr uint16_t implicitTlsPort = 465;
// A fatal error blocks the thread that reported it until the mail is out, so
// every network step needs a ceiling well below "the user gives up".
constexpr int socketTimeoutSeconds = 15;
// A greeting or error text beyond this is a broken (or hostile) server, not
// something worth buffering.
constexpr size_t maxReplySize = 4096;

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

// RFC 2045 caps a line at 76 characters; 57 input bytes encode to exactly that.
string base64Lines(const string& data)
{
	constexpr size_t bytesPerLine = 57;
	string encoded;
	for (size_t pos = 0; pos < data.size(); pos += bytesPerLine)
		encoded += base64(data.substr(pos, bytesPerLine)) + "\r\n";
	return encoded;
}

// RFC 2047 encoded word. Applied unconditionally: subjects come from the logger
// and stay far below the 75 character limit of a single encoded word.
string encodedWord(const wstring& text)
{
	return "=?UTF-8?B?" + base64(wstring_to_string(text)) + "?=";
}

string rfc5322Date()
{
	static const char* const days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char* const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	const std::time_t now = std::time(nullptr);
	std::tm utc{};
#ifdef WIN32
	gmtime_s(&utc, &now);
#else
	gmtime_r(&now, &utc);
#endif
	// Spelled out instead of strftime's %a/%b: those follow the global locale,
	// which the GUI switches to the user's language - a German weekday name
	// makes the header unparsable.
	char date[64];
	std::snprintf(date, sizeof date, "%s, %02d %s %04d %02d:%02d:%02d +0000",
		days[utc.tm_wday], utc.tm_mday, months[utc.tm_mon], utc.tm_year + 1900,
		utc.tm_hour, utc.tm_min, utc.tm_sec);
	return date;
}

string localHostName()
{
	char name[256];
	if (gethostname(name, sizeof name) != 0)
		return "localhost";
	name[sizeof name - 1] = '\0';
	return name[0] == '\0' ? "localhost" : name;
}

#ifdef WIN32
// The vcpkg build of OpenSSL looks for a PEM bundle at a path that does not
// exist on Windows, so peer verification would fail against every server. The
// roots Windows already trusts are copied into the store instead.
void loadSystemRootCertificates(SSL_CTX* ctx)
{
	HCERTSTORE systemStore = CertOpenSystemStoreW(0, L"ROOT");
	if (!systemStore)
		return;
	X509_STORE* trusted = SSL_CTX_get_cert_store(ctx);
	PCCERT_CONTEXT entry = nullptr;
	while ((entry = CertEnumCertificatesInStore(systemStore, entry)) != nullptr) {
		const unsigned char* encoded = entry->pbCertEncoded;
		// An entry OpenSSL cannot parse is skipped, not treated as an error: the
		// system store also holds formats it does not know.
		if (X509* certificate = d2i_X509(nullptr, &encoded, entry->cbCertEncoded)) {
			X509_STORE_add_cert(trusted, certificate);
			X509_free(certificate);
		}
	}
	CertCloseStore(systemStore, 0);
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
private:
	bool connect();
	void setTimeouts();
	bool enableTls();
	bool upgradeToTls();
	bool ehlo();
	bool authenticate();
	bool deliver(const wstring& recipient, const wstring& subject, const string& body);
	string message(const wstring& recipient, const wstring& subject, const string& body) const;
	bool put(const string& line);
	bool readReply(int& code);
	bool reply(int expected);
	bool replyOk();
	bool fail(const string& reason) { error_ = reason; return false; }

	const SmtpAccount& account_;
	string	 host_;
	BIO*	 bio_ = nullptr;	// the whole chain once TLS is on
	SSL_CTX* ctx_ = nullptr;
	string	 error_;
	string	 lastReply_;
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
		return fail("Verbindung zu " + address + " konnte nicht angelegt werden");
	// Blocking: the timeouts below cover reads and writes, the connect attempt
	// itself is limited by the operating system.
	if (BIO_do_connect(bio_) <= 0)
		return fail("keine Verbindung zu " + address);
	setTimeouts();
	return true;
}

void SmtpSession::setTimeouts()
{
	const int socketHandle = BIO_get_fd(bio_, nullptr);
	if (socketHandle < 0)
		return;
#ifdef WIN32
	DWORD timeout = socketTimeoutSeconds * 1000;
	const char* value = reinterpret_cast<const char*>(&timeout);
	int size = sizeof timeout;
	SOCKET socketId = static_cast<SOCKET>(socketHandle);
#else
	timeval timeout{ socketTimeoutSeconds, 0 };
	const void* value = &timeout;
	socklen_t size = sizeof timeout;
	int socketId = socketHandle;
#endif
	setsockopt(socketId, SOL_SOCKET, SO_RCVTIMEO, value, size);
	setsockopt(socketId, SOL_SOCKET, SO_SNDTIMEO, value, size);
}

bool SmtpSession::enableTls()
{
	ctx_ = SSL_CTX_new(TLS_client_method());
	if (!ctx_)
		return fail("TLS konnte nicht initialisiert werden");
	SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
	// Verified, not just encrypted: the connection carries the mail account
	// password, so an intercepting server must not be accepted.
	SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
	SSL_CTX_set_default_verify_paths(ctx_);
#ifdef WIN32
	loadSystemRootCertificates(ctx_);
#endif
	BIO* tls = BIO_new_ssl(ctx_, 1);
	if (!tls)
		return fail("TLS-Schicht konnte nicht angelegt werden");
	SSL* ssl = nullptr;
	BIO_get_ssl(tls, &ssl);
	if (!ssl) {
		BIO_free(tls);
		return fail("TLS-Schicht konnte nicht angelegt werden");
	}
	SSL_set_tlsext_host_name(ssl, host_.c_str());	// servers with several certificates need SNI
	SSL_set1_host(ssl, host_.c_str());				// makes the handshake check the certificate name
	bio_ = BIO_push(tls, bio_);
	if (BIO_do_handshake(bio_) <= 0)
		return fail("TLS-Handshake mit " + host_ + " fehlgeschlagen (Zertifikat pruefen)");
	// The SSL BIO has no gets method: readReply() would get -2 from every reply
	// once the connection is encrypted. A buffer BIO on top supplies the
	// line-wise read the plain connection BIO offered before.
	BIO* buffered = BIO_new(BIO_f_buffer());
	if (!buffered)
		return fail("Lesepuffer fuer " + host_ + " konnte nicht angelegt werden");
	bio_ = BIO_push(buffered, bio_);
	return true;
}

// A plain local relay (port 25, no login) may not offer STARTTLS. That is
// tolerated only as long as no password is configured - a password must never
// travel over an unencrypted connection.
bool SmtpSession::upgradeToTls()
{
	if (!put("STARTTLS"))
		return false;
	int code = 0;
	if (!readReply(code))
		return false;
	if (code != 220)
		return account_.password.empty() ? true : fail(host_ + " bietet kein STARTTLS: " + lastReply_);
	// The capability list is only valid for the current encryption state, so the
	// greeting is repeated inside the tunnel.
	return enableTls() && ehlo();
}

bool SmtpSession::ehlo()
{
	return put("EHLO " + localHostName()) && replyOk();
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
		return fail("Anmeldung an " + host_ + " abgelehnt");
	return true;
}

string SmtpSession::message(const wstring& recipient, const wstring& subject, const string& body) const
{
	std::ostringstream os;
	os << "From: <" << wstring_to_string(account_.from) << ">\r\n"
	   << "To: <" << wstring_to_string(recipient) << ">\r\n"
	   << "Subject: " << encodedWord(subject) << "\r\n"
	   << "Date: " << rfc5322Date() << "\r\n"
	   << "MIME-Version: 1.0\r\n"
	   << "Content-Type: text/plain; charset=UTF-8\r\n"
	   << "Content-Transfer-Encoding: base64\r\n"
	   << "\r\n"
	   << base64Lines(body);
	return os.str();
}

bool SmtpSession::deliver(const wstring& recipient, const wstring& subject, const string& body)
{
	if (!put("MAIL FROM:<" + wstring_to_string(account_.from) + ">") || !replyOk())
		return false;
	if (!put("RCPT TO:<" + wstring_to_string(recipient) + ">") || !replyOk())
		return false;
	if (!put("DATA") || !reply(354))
		return false;
	// No dot stuffing: header lines never start with a dot and the base64
	// alphabet of the body does not contain one.
	if (!put(message(recipient, subject, body) + "\r\n."))
		return false;
	return replyOk();
}

string SmtpSession::send(const wstring& recipient, const wstring& subject, const string& body)
{
	const bool implicitTls = account_.port == implicitTlsPort;
	if (!connect())
		return error_;
	if (implicitTls && !enableTls())
		return error_;
	if (!reply(220) || !ehlo())
		return error_;
	if (!implicitTls && !upgradeToTls())
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
			return fail("Senden an " + host_ + " fehlgeschlagen");
		written += chunk;
	}
	BIO_flush(bio_);
	return true;
}

bool SmtpSession::readReply(int& code)
{
	char buffer[512];
	for (;;) {
		int length = BIO_gets(bio_, buffer, sizeof buffer);
		if (length <= 0)
			return fail("keine Antwort von " + host_);
		string line(buffer, length);
		while (line.back() != '\n' && line.size() < maxReplySize) {	// reply longer than the buffer
			length = BIO_gets(bio_, buffer, sizeof buffer);
			if (length <= 0)
				return fail("abgebrochene Antwort von " + host_);
			line.append(buffer, length);
		}
		while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			line.pop_back();
		if (line.size() < 3 || !std::isdigit(static_cast<unsigned char>(line[0]))
			|| !std::isdigit(static_cast<unsigned char>(line[1])) || !std::isdigit(static_cast<unsigned char>(line[2])))
			return fail("unerwartete Antwort von " + host_ + ": " + line);
		code = std::stoi(line.substr(0, 3));
		lastReply_ = line;
		if (line.size() == 3 || line[3] != '-')	// '-' marks a continuation line
			return true;
	}
}

bool SmtpSession::reply(int expected)
{
	int code = 0;
	if (!readReply(code))
		return false;
	return code == expected ? true : fail(host_ + " antwortet: " + lastReply_);
}

bool SmtpSession::replyOk()
{
	int code = 0;
	if (!readReply(code))
		return false;
	return code >= 200 && code < 300 ? true : fail(host_ + " antwortet: " + lastReply_);
}

} // namespace

std::string sendMail(const SmtpAccount& account, const std::wstring& recipient,
	const std::wstring& subject, const std::string& body)
{
	if (!account.complete())
		return "SMTP-Server oder Absender ist nicht konfiguriert";
	if (recipient.empty())
		return "kein Empfaenger";
	try {
		SmtpSession session(account);
		return session.send(recipient, subject, body);
	}
	catch (const std::exception& ex) {
		// The caller is the logger, reached from catch handlers and worker
		// threads; an exception escaping here would terminate the process while
		// it is already reporting a fatal error.
		return ex.what();
	}
}
