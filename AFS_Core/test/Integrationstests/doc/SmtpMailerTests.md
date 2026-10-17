# SmtpMailerTests

Module comment for SmtpMailerTests.cpp.

The mail notification had no test at all: send_mail() needs a server, and Logging::notify_fatal()
is reached only through log_fatal(). SmtpSink (SmtpSink.md) supplies the server inside the test
process, so both layers are tested end to end - the SMTP dialogue and the decision whether to
send. Unix only, like the sink.

## port_nobody_listens_on

A port the kernel just handed out and that was closed again: nothing listens there, so the
connect fails at once and locally - no DNS, no timeout.

## unique

notify_again() remembers the last notified text in the singleton for 30 minutes and other
tests in the same process share it; a message that carries the pid and the test name can never
be mistaken for an earlier one.

## IncompleteAccountIsRefusedBeforeAnyConnection

The two guard clauses of send_mail() answer with their message and without touching the
network - the sink must not see a connection.

## MailReachesAPlainServerWithoutPassword

The whole happy path over a plain connection: envelope, UTF-8 subject as encoded word, UTF-8
body as base64 lines, the STARTTLS attempt before anything else, QUIT at the end.

## PasswordIsNeverSentWithoutTls

The security property of upgrade_to_tls(): a server that refuses STARTTLS gets no AUTH and no
mail when the account has credentials - a plain-text login would leak the password.

## FatalMailTest

Configures the notification against the sink and keeps the process-wide Logging state out of
the other tests (ScopedLoggingSetup.md). The log is redirected so FailedDeliveryIsLoggedAsAnError
can read what notify_fatal() logged.

## catalogue

Run alone, the test binary has no catalogue loaded and, in the C locale, gettext hands back the
bare message ids - "notification_failed" without its %1 and thus without the recipient the test
looks for. The fixture loads the locale and the English catalogue the way TranslationTests do
and puts both back in TearDown; where no usable locale exists at all, only the recipient
assertion is left out, the rest of the test still runs.

## FatalErrorIsMailedToTheConfiguredAddress

log_fatal() sends synchronously (Logging.md#mail-sent-synchronously), so the mail is complete
once wait_for_connections() has seen the session end.

## TheSameFatalErrorIsMailedOnlyOnce

Logging.md#notify_again: the daemon repeats the same failing sync every cycle; the text is
compared without its timestamp, so the identical message stays silent while a different one goes
out.

## nothing-without-consent

Three ways to have nothing sent - notification off, no address, incomplete account - and each
one must leave the sink without a connection: an install must not mail anywhere unasked.

## FailedDeliveryIsLoggedAsAnError

The sender cannot log through the mailer (it would recurse), so notify_fatal() logs the failure
with the recipient and the server's answer; the fatal line itself is not lost either.
