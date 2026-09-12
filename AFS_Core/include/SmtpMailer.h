#pragma once
#include <cstdint>
#include <string>

namespace AFS_Core {

struct SmtpAccount {  // [SmtpMailer.md#SmtpAccount]
	std::wstring server;
	uint16_t     port = 587;	// submission port; 465 means implicit TLS
	std::wstring user;			// empty: server accepts unauthenticated relaying
	std::wstring password;
	std::wstring from;
	bool complete() const { return !server.empty() && !from.empty(); }
	bool operator==(const SmtpAccount&) const = default;
};

std::string send_mail(const SmtpAccount& account, const std::wstring& recipient,
	const std::wstring& subject, const std::string& body);  // [SmtpMailer.md#send_mail]

} // namespace AFS_Core
