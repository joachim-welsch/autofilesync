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

## plaintext-without-password

A server that refuses STARTTLS still gets the mail when the account has no password: a relay on
the local network often has no certificate, and a notification that never arrives helps nobody.
The session remembers that it went unencrypted and send_mail() reports it in MailResult, so the
caller can say so in the log - the mailer itself must not log (see send_mail).

## sealed_password

The stored ciphertext of a password that could not be decrypted, kept so that a save does not
replace it with an empty one; see SshAccount.md#sealed_password_. A dialog that builds a new
account leaves it empty, so a password typed in again takes its place.
