# LogAnalysisTests

Module comment for LogAnalysisTests.cpp.

## ScopedLogPath

Points Logging at a temporary file for the duration of the test and restores the previous path
afterwards, mirroring LogCutTests.cpp.

## TruncatesToWholeLinesWhenOverBudget

The excerpt must start at a real log entry, not mid-line: a truncated first line would
misrepresent the entry it belongs to.

## WarningsQuestionIsRecognisedAndFindsWarningEntries

The chat question ChatDialog::onSend() actually routes through the log path for: detection must
fire, and the excerpt it attaches to the request must contain the warning entries the question
asks about.

## LogWordsTest

The words besides "log" come from the catalogue, so these tests load the catalogues and switch
the language as the settings do; the locale and LANGUAGE are put back afterwards, so that the
tests after them see the bare keys as before.

## RecognisesTheLogWordOfEveryLanguage

Regression test: only "log" and "protokoll" were recognised, so a question asked in Spanish,
French, Italian, Polish or Ukrainian with the native word got no log excerpt. None of these
questions contains "log".
