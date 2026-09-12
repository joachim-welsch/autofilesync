# Logging

Module comment for Logging.h / Logging.cpp.

## notify_email_active_

Recipient for the levels that need a person to act (see LogLevel). Off by default: an install
must not start sending anywhere unasked.

## env_var

Reads the Win32 environment block instead of the CRT cache so that values set at runtime are
seen.

## windows-log-location

As on Linux (~/.cache/autofilesync) the log belongs in the user's own data directory, not in
%TEMP%: cleanup tools purge that, and all users of the machine would share one file name.
Logging::create_dir() creates it on demand.

## get_default_log_file_path

Logging.h's non-WIN32 default used to be a hardcoded developer path
(/home/welschj/temp/autofilesync.log) - the equivalent of this function simply was never written
for Linux/macOS, so it fell back to a literal path that exists on exactly one machine. On any
other install, logging silently produced nothing (open_file() failing is now tolerated, not
fixed) unless a config file already existed and happened to override it.

## count_enum_elements

Counts from the last enumerator, so the table below covers every level. Leaving None out made
log_level_str(LogLevel::None) read past the end.

## NOTIFY_REPEAT_INTERVAL

How long the same fatal error stays silent after it was reported once.

## create_dir-never-throws

The error_code overload: a path whose parent conflicts with an existing non-directory file (bad
user input, a stale file left where a directory should be) must not throw here - open_file()
below already tolerates fopen() failing afterward and must get the chance to.

## open_file-never-throws

Never throw: an invalid/unwritable path (bad user input in LoggingDialog, a directory removed
since the last run) must not turn into an uncaught exception. set_log_file_path_name() calls
this after already close_file()-ing the previous handle, and LogCut::close_files() calls it
unconditionally from MainWindow's constructor on every single startup - either path throwing
would crash the whole application instead of just leaving logging disabled (write() already
handles fos_ == nullptr gracefully).

## log-level-from-string

"None" is not accepted: a configuration that switches the log off entirely would be
indistinguishable from a typo, which gets the safe default below - Error keeps the levels that
need a person to act.

## write_to_file-locking

Locked for the whole call: close_file()/open_file() (log rotation, called concurrently from
GUI/worker threads) must not fclose() fos_ out from under an in-flight write, or this
dereferences a dangling/null FILE*.

## write-failures-dropped

Never throw from here: LOG_ macros run inside catch handlers and worker threads - an exception
escaping them terminates the process exactly when something else (e.g. a full disk) is already
going wrong. Failures are silently dropped; there is nowhere sensible left to report them.

## write-notifies-outside-lock

Notified outside write_to_file(): that call holds mutex_, which every accessor below takes as
well, and sending blocks on the network.

## error_text

Strips the timestamped head write_head() put in front, so that the same error reported again is
recognised as the same error.

## mail-sent-synchronously

Sent synchronously although it delays the caller: a fatal error is often followed by the process
going down, and a detached thread would be killed with it before the mail is on its way.

## notify_again

Identical errors repeat: the daemon retries the same failing sync every cycle and would
otherwise send one mail per cycle. A different error always passes.
