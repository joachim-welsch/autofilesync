# HelpKnowledge

Module comment for HelpKnowledge.h / HelpKnowledge.cpp.

## help_knowledge

The help pages as a source for the assistant: the system prompt only says what AutoFileSync is,
so "how do I ...?" was answered from the model's guesses. The section of the help that fits the
question is sent along with it, the same way as the log excerpt. Plain text processing, free of
wxWidgets; the desktop program reads the pages out of the help book (HelpBook).

A keyword match, not a search engine: it finds a section only where the question uses the
help's own words. The chat reads the help book of the language shown (see
MainWindow.md#help_language), so a question finds its section when it is asked in the language
the program shows; a question in another language mostly finds nothing and is answered as
before. A wrong section costs little - the instruction tells the model to pass
over an excerpt that does not fit.

## sections_of

One section per subheading, plus the text before the first one. A whole page would often not
fit next to the conversation, and a section is what a question is usually about. The page title
comes from <h1>, so that the answer can name the page the user finds in the help.

## plain_text

The model reads text, not markup: tags go, paragraphs, list items and table rows become lines,
table cells are separated by a space, and runs of whitespace collapse.

## entity

The entities the help pages use, the German letters for hand-written pages, and numeric
references. Anything else stays as written.

## word_char

Every character above ASCII counts as a letter, so that German words are not torn apart at
their umlauts in a locale where iswalnum() knows only ASCII.

## stems_of

Words of five letters and more - shorter ones such as "geht" or "dir" are mostly filler and
once matched a rare section by chance - and acronyms in capitals from three letters, such as
"SSH". Each word loses up to two letters of ending, so that "lösche" finds "löschen" and
"gelöscht".

## scored

A stem in the page title or the subheading scores TITLE_SCORE: the help has a page or a section
for most topics, and its title names it. A stem in the text scores by how rare it is among the
sections (inverse document frequency), so that words found on every page count little.

## MIN_SCORE

At least one title match, or text matches about as specific as one. Below that the question is
not about anything the help describes.

## second-section

A second section that scores as well and still fits the budget goes along: questions often
touch two neighbouring topics, and the 2500 characters leave room.
