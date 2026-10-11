# TranslationTests

Module comment for TranslationTests.cpp.

## LanguageWithoutCatalogueFallsBackToEnglish

French has no catalogue. The message ids are technical keys, so without the English fallback in
translation::init() a French session read "already_running" instead of a sentence. LANGUAGE
stands in for the session language: gettext puts it before LC_MESSAGES, and the test machine
need not have a French locale generated.
