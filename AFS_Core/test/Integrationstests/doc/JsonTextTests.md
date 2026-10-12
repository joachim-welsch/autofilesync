# JsonTextTests

Module comment for JsonTextTests.cpp.

## KeepsEveryCharacterReadable

Cyrillic stands for everything above U+00FF, which write_json() escaped, and the emoji for
everything above U+FFFF, which it clamped to ￿ and so lost. No \u may be left in the text,
and boost's reader must get the same string back.

## LayoutMatchesWriteJson

Files written before and after the switch should look alike and diff cleanly, so the layout is
compared with write_json() itself on a tree that needs no escaping: nested objects, an array of
objects and an empty value.
