# LoggingCollectTests

Module comment for LoggingCollectTests.cpp.

## CollectingStopped

Collecting is process-wide state of the Logging singleton; a test that fails halfway must not
leave it on for the tests after it, so it is switched off and emptied when the test leaves
its scope.

## below-the-level

The point of collecting independently of the log level: the GUI shows the infos of a run on
request while the log stays at Error. The file proves the level still holds for what is
written.

## cap

MAX_COLLECTED in Logging.cpp; the number here is that constant, so a change there has to be
reflected here on purpose.
