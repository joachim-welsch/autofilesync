# LlamaClientTests

Module comment for LlamaClientTests.cpp.

## OldestRoundsGoFirst

The size of the history stands in for the token count, so that the shortening is tested
without a model. Regression: a long conversation used to fail with every further question
instead of forgetting its beginning.
