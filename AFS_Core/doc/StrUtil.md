# StrUtil

Module comment for StrUtil.h / StrUtil.cpp.

## utf8-conversions-never-throw

Hand-written instead of std::wstring_convert: that one throws std::range_error on the first byte
that is not valid UTF-8, and file names on Linux are arbitrary bytes. These conversions run
inside every log call, including the ones in catch handlers, so they must never throw; invalid
input becomes U+FFFD instead.

## decode_utf8

Decodes one code point starting at s[pos] and advances pos past it.
