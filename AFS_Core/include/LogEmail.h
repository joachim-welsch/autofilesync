#pragma once

class LogEmail {
	mutable std::mutex 		mutex_;
	bool					notify_active_ = false;  // [Logging.md#notify_email_active_]
	std::wstring			notify_email_;
	SmtpAccount				smtp_;  // the account the notification is sent through
public:
	void notify_active(bool active));
};
