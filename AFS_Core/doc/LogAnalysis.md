# LogAnalysis

Module comment for LogAnalysis.h / LogAnalysis.cpp.

## read_recent_log_entries

Reads the tail of the configured log file (Logging::instance().get_log_file_path_name()) for the
chat assistant to analyse. Kept separate from LlamaClient: it is plain file I/O and testable
without loading a model. maxChars bounds the excerpt so it fits the model's context window
alongside the system prompt and the analysis instruction; the excerpt starts at the next full
line, never mid-entry. Returns an empty string if the file is missing, unreadable, or empty.

## looks_like_log_question

True if text plausibly asks about the log/log file. Case-insensitive substring match against
log_words(): a chat question isn't worth a real NLU pass just to decide whether to attach the
log.

## log_words

"log" is understood everywhere as a loanword; the native words come from the catalogue of the
current language ("Protokoll", "journal", "registro", "dziennik", "журнал"), comma-separated in
log_question_words. With only "log" and "protokoll" built in, a question in five of the seven
languages got no log excerpt, and the model guessed instead.

## towlower

towlower() lowers non-ASCII letters only in a UTF-8 locale - which the programs set in
init_locale(); in the "C" locale an uppercase "Журнал" would not match. The catalogue words are
lowercase.
