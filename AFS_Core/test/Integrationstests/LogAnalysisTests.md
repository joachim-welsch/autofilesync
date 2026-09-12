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
