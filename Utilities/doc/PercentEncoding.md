# PercentEncoding

Module comment for PercentEncoding.h / PercentEncoding.cpp.

## encoded

Percent-encoding after RFC 3986 of one path segment or one query value: every byte except the
unreserved characters (ASCII letters, digits, "-", ".", "_", "~") becomes "%" and two uppercase
hex digits. "/" is encoded too, so a whole path has to be encoded segment by segment. The
characters are tested by hand instead of with isalnum(), which follows the locale and would let
bytes above 127 through unencoded.

## decoded

The reverse. A "%" that is not followed by two hex digits stays as it is, so text that was never
encoded passes through unharmed; "+" is not turned into a space.
