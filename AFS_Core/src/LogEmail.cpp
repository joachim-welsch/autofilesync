#include "LogEmail.h"


namespace AFS_Core {

void LogEmail::notify_active(bool active)
{ 
	std::lock_guard<std::mutex> lock(mutex_);
	notify_active_ = active; 
}

bool LogEmail::notify_active() const
{ 
	std::lock_guard<std::mutex> lock(mutex_); 
	return notify_active_;
}

std::wstring LogEmail::notify_adress() const 
{ 
	std::lock_guard<std::mutex> lock(mutex_); return notify_adress_; 
}

void  LogEmail::notify_adress(const std::wstring& address) 
{ 
	std::lock_guard<std::mutex> lock(mutex_); 
	notify_adress_ = address; 
}

SmtpAccount LogEmail::smtp_account() const 
{ 
	std::lock_guard<std::mutex> lock(mutex_); 
	return smtp_; 
}

void LogEmail::smtp_account(const SmtpAccount& account) 
{ 
	std::lock_guard<std::mutex> lock(mutex_);
	smtp_ = account; 
}

} // namespace AFS_Core