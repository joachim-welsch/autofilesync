# HelpBook

Module comment for HelpBook.h / HelpBook.cpp.

## read_help_book

The sections of every HTML page in the help book, for the assistant (see
HelpKnowledge.md#help_knowledge). The book is the zip file the help window shows, so the
assistant knows exactly the help that was shipped. An empty path or an unreadable book gives no
sections: the assistant then answers without help, as before.

## quiet

wxWidgets reports an unreadable file through wxLog, which would pop up a message box while the
chat window opens; a missing help book is no reason for that.
