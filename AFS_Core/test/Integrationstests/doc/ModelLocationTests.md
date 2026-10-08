# ModelLocationTests

Module comment for ModelLocationTests.cpp.

## REDIRECTED_VARIABLES

The variables model_location derives the model path from: AUTOFILESYNC_LLAMA_MODEL itself, plus
LOCALAPPDATA and USERPROFILE on Windows, HOME and XDG_DATA_HOME elsewhere. A developer machine
usually has the model variable set, so it is saved and cleared like the rest.

## ModelLocationTest

Points the environment at a fresh temp directory so the lookup can be exercised without touching
the real models directory - a model of several gigabytes lives there.

## IgnoresPartialDownloadsAndOtherFiles

The setup scripts download to `<name>.gguf.part`; picking that up would hand llama.cpp a
truncated file. A directory that happens to end in `.gguf` is no model either.

## VariableTakesPrecedenceOverDirectory

The configured file does not exist on purpose: the variable must win anyway, so a wrong path
surfaces as "cannot load" instead of quietly loading another model.
