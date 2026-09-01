#pragma once
#include <cstddef>
#include <string>

// Reads the tail of the configured log file (Logging::instance().getLogFilePathName())
// for the chat assistant to analyse. Kept separate from LlamaClient: it is
// plain file I/O and testable without loading a model.
// maxChars bounds the excerpt so it fits the model's context window alongside
// the system prompt and the analysis instruction; the excerpt starts at the
// next full line, never mid-entry. Returns an empty string if the file is
// missing, unreadable, or empty.
std::wstring readRecentLogEntries(std::size_t maxChars);

// True if text plausibly asks about the log/log file - German uses both "Log"
// (loanword) and "Protokoll" (native term, as in LoggingDialog's
// "Protokolldatei-Pfad"). Case-insensitive substring match: a chat question
// isn't worth a real NLU pass just to decide whether to attach the log.
bool looksLikeLogQuestion(const std::wstring& text);
