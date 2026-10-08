# SmtpMailer

Module comment for SmtpMailer.h / SmtpMailer.cpp.

## SmtpAccount

Mail server and sender identity, as configured under "Logging" in the config file. Kept out of
Logging so the SMTP code can be used (and tested) without the logger singleton.

## send_mail

Sends one mail synchronously; returns an empty string on success, otherwise the reason. Failures
are returned instead of logged because the only caller is the logger itself - logging from here
would recurse. The body is UTF-8 and stays a narrow string on purpose: it comes from the log
line, which is UTF-8 already, and the detour through wstring loses every non-ASCII character on
Windows.
