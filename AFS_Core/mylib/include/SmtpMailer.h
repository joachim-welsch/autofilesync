#pragma once
#include <cstdint>
#include <string>

// Mail server and sender identity, as configured under "Logging" in the config
// file. Kept out of Logging so the SMTP code can be used (and tested) without
// the logger singleton.
struct SmtpAccount {
	std::wstring server;
	uint16_t     port = 587;	// submission port; 465 means implicit TLS
	std::wstring user;			// empty: server accepts unauthenticated relaying
	std::wstring password;
	std::wstring from;
	bool complete() const { return !server.empty() && !from.empty(); }
	bool operator==(const SmtpAccount&) const = default;
};

// Sends one mail synchronously; returns an empty string on success, otherwise
// the reason. Failures are returned instead of logged because the only caller
// is the logger itself - logging from here would recurse.
// The body is UTF-8 and stays a narrow string on purpose: it comes from the log
// line, which is UTF-8 already, and the detour through wstring loses every
// non-ASCII character on Windows.
std::string sendMail(const SmtpAccount& account, const std::wstring& recipient,
	const std::wstring& subject, const std::string& body);
