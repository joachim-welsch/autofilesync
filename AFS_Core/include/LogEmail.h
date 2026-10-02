#pragma once
#include <string>
#include <mutex>
#include "SmtpMailer.h"

namespace AFS_Core {

class LogEmail {
	mutable std::mutex 		mutex_;
	bool					notify_active_ = false;  // [Logging.md#notify_email_active_]
	std::wstring			notify_adress_;
	SmtpAccount				smtp_;  // the account the notification is sent through
public:
	void notify_active(bool active);
	bool notify_active() const;
	std::wstring notify_adress() const;
	void notify_adress(const std::wstring& address);
	void smtp_account(const SmtpAccount& account);
	SmtpAccount smtp_account() const;
};

} // namespace AFS_Core