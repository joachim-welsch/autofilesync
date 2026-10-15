# HelpKnowledgeTests

Module comment for HelpKnowledgeTests.cpp.

## real_help

The help pages of the repository, read straight from AFS_Desktop/help rather than from the
packed help book, which only the desktop program knows how to open.

## SmallTalkGetsNoExcerpt

A question without a word the help is about must not drag in a section: the model would talk
about it. Short words such as "geht" once matched a rare section by chance.

## TypicalQuestionsFindTheirHelpPage

Pins the ranking on the real help, so that a change to the weights or to the help shows up
here. Each language's questions are tested against that language's book; questions in a
language the book is not written in find nothing, see HelpKnowledge.md#help_knowledge.

## EveryOfferedLanguageHasACompleteHelpBook

Every language the settings offer is meant to have its help. The German book is the original;
each translation must have the same files and the same links, so that no page is missing and
no link of the contents, the index or the text leads nowhere.
