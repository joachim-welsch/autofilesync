# ScopedLoggingSetup

Module comment for ScopedLoggingSetup.h.

## ScopedLoggingSetup

The Logging section of the configuration and the mail notification live in the process-wide
Logging singleton, so a test that reads a configuration or configures a mail account changes
the log level, log file, retention, editor and mail settings of every test that follows. The
guard takes a snapshot where it is constructed and restores it when the test body ends, before
TearDown removes a test home the log file may point into. A fixture that keeps the guard as a
member has to end it in TearDown itself (std::optional), because members outlive TearDown. Shared by ConfigTests and
SmtpMailerTests.
