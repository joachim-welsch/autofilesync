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

Logging.h's non-WIN32 default used to be a hardcoded path in one developer's home
directory - the equivalent of this function simply was never written
for Linux/macOS, so it fell back to a literal path that exists on exactly one machine. On any
other install, logging silently produced nothing (open_file() failing is now tolerated, not
fixed) unless a config file already existed and happened to override it.

## count_enum_elements

Counts from the last enumerator, so the table below covers every level. Leaving None out made
log_level_str(LogLevel::None) read past the end.

## NOTIFY_REPEAT_INTERVAL

How long the same fatal error stays silent after it was reported once.

## repetitions_

The messages from Warning upwards that were written lately, without their timestamped head, each
with the time it was written and how often it has been held back since.

## report_again

The daemon retries a failing sync every cycle - an SSH host that is switched off produced one
identical line per second. The same message is therefore written once and then held back for
LOG_REPEAT_INTERVAL; the next one after that carries the number of held back repetitions. The
messages are remembered by their text and not only the last one, because the failing source and
target of one item alternate. Debug and Info are never held back: they report progress, and the
same line twice there means it happened twice.

## LOG_REPEAT_INTERVAL

How long the same warning or error stays silent after it was written once.

## MAX_REPETITIONS

Bounds repetitions_: a failing tree produces one distinct message per file. Beyond the bound the
expired entries go, and if that is not enough all of them - the price is one repeated line.

## repetitions-follow-the-file

A new or emptied log file starts without history, so the first occurrence is in the file the
reader looks at.

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

## unencrypted-mail

A notification that went over a plain connection is logged as a warning naming the server: the
mail carries file names and error texts, and the user should know they left the machine
readable (SmtpMailer.md#plaintext-without-password). A warning, not an error: nothing failed.

## notify_again

Identical errors repeat: the daemon retries the same failing sync every cycle and would
otherwise send one mail per cycle. A different error always passes.

## LogOrigin

The GUI program and the daemon share one configuration and therefore one log
file. Each line carries the writer in front of the level - G or D, the value of
the enumerator - so that a line can be attributed without guessing from its
text. The GUI is the default; the daemon announces itself before its first
message. The readers of the file (LogCut, LogAnalysis) only parse the timestamp
at the start of a line, which the marker leaves untouched.

## follow_replaced_file

The GUI program and the daemon write to the same file, and LogCut in either of
them replaces it by renaming a shortened copy over it. The handle of the other
process then still points to the old, now nameless file, and everything it
writes is lost until it restarts. Before each write the open handle is therefore
compared with the file the path names now (device and inode); when they differ,
or the path is gone, the file is opened anew. A handle that is closed on purpose
- LogCut of the own process is at work - stays closed. The check costs one
fstat() and one stat() per message, little next to the fsync() that follows.
Windows needs none of this: there a file that another process holds open cannot
be replaced, so LogCut writes the shortened log back into the existing file
(LogCut.md#replace_log_file) and the handle stays valid.

## collected_

The messages of one run, collected for the GUI, which shows them as a summary box after
"Einmal synchronisieren" according to the user's Fehler/Warnung/Info filters. Collecting is
independent of the log level: a user who keeps the log at Error can still ask for the infos
of a run in a box, so should_log() lets Info and above through while collecting and
write_to_file() applies the level itself. The head (timestamp, origin, level) is stripped and
the line break dropped - the box shows the text, the level goes with it as a value. Capped at
MAX_COLLECTED so a run over thousands of files cannot grow the vector without bound; the box
tells the user to look into the log for the rest.

## wide-file-names

On Windows a narrow file name is read in the ANSI code page of the system, while
wstring_to_string() yields UTF-8. A user profile with an umlaut in its name - the log lies
below it - would therefore name a file that does not exist, and the program would run without a
log. The wide call takes the name as it is.
