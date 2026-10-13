# TranslationTests

Module comment for TranslationTests.cpp.

## LanguageWithoutCatalogueFallsBackToEnglish

Swedish has no catalogue. The message ids are technical keys, so without the English fallback in
translation::init() a Swedish session read "already_running" instead of a sentence. LANGUAGE
stands in for the session language: gettext puts it before LC_MESSAGES, and the test machine
need not have a Swedish locale generated.

## NoChosenLanguageReturnsToEnvironment

Withdrawing a choice must restore the language the environment named at init(), not the one
chosen last nor English: set_language() overwrites LANGUAGE, so init() has to keep the original.

## CatalogueOfThisBuildMatches

The catalogues beside the test binary were compiled by the same build, so the stamps agree;
this is the guard against the build step and the program drifting apart (a renamed entry, a
stamp computed from different files), which would make every installation report a mismatch.

## MissingTranslationIsLoggedOnce

A message id unknown to the catalogue yields one warning naming it, not one per call. The id
carries a clock value because the reported ids are remembered for the whole process - a fixed
id would already be known in a repeated run.
