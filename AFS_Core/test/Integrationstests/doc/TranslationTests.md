# TranslationTests

Module comment for TranslationTests.cpp.

## LanguageWithoutCatalogueFallsBackToEnglish

Swedish has no catalogue. The message ids are technical keys, so without the English fallback in
translation::init() a Swedish session read "already_running" instead of a sentence. LANGUAGE
stands in for the session language: gettext puts it before LC_MESSAGES, and the test machine
need not have a Swedish locale generated.
