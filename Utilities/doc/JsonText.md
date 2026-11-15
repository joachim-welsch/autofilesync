# JsonText

Module comment for JsonText.h / JsonText.cpp.

## json

The configuration and the backup proofs are meant to be read by people, in a text editor, in
their language. boost's write_json() escapes every character above U+00FF as \uXXXX - a
Ukrainian proof was unreadable - and clamps characters above U+FFFF to ￿, which loses an
emoji in a name for good on Linux. Reading stays with boost's read_json(), which takes both the
plain characters and the escapes of files written before.

## to_text

The same layout as write_json(..., pretty = true) - four spaces per level, "key": value, arrays
for children without names - so that files written before and after look alike and diff
cleanly. The text is wide; util::write_utf8_file() turns it into UTF-8.

## escaped

Only what JSON requires: the quote, the backslash and the control characters below U+0020.
write_json() also escaped '/' as "\/", which JSON allows but never requires; paths read better
without it.

## representable

The same check as boost's verify_json(): JSON has no node that is both a value and an object,
and the root must be an object. Throwing keeps write_json()'s behaviour, where the callers
already report a failed write.

## write

A node without children is a string value, one whose children all have empty names is an array,
anything else an object - write_json()'s rule. The root is always an object, even when empty.
