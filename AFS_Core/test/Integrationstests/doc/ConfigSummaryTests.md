# ConfigSummaryTests

Module comment for ConfigSummaryTests.cpp.

Expectations are built with translate() like the summary itself, so they hold with the
catalogue bound (the words of the current language) and without it (the bare message ids).

## only-enabled-options

An option that is off must not be named: the model would otherwise read "Generationenprinzip"
in the line and report the option as set.

## SameConfigurationRendersIdentically

The summary is part of the prompt prefix the KV cache reuses; any nondeterminism (unordered
containers, timestamps) would make every request decode the whole prefix again.
