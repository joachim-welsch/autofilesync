# Base64

Module comment for Base64.h / Base64.cpp.

## alphabets

Base64 after RFC 4648, written by hand so that the module needs no OpenSSL and is available in
every build of the core. encoded() uses the standard alphabet ("+" and "/") and pads with "=" to a
multiple of four characters; encoded_unpadded() is the same text without the padding, the form
OpenSSH prints for key fingerprints.

## url_encoded

The alphabet of RFC 4648 section 5 ("-" and "_" instead of "+" and "/") without padding, so the
text can stand in a URL or a file name unescaped.

## decoded

Reads the standard alphabet with padding. Returns an empty vector for anything that is not valid:
a length that is no multiple of four, a character outside the alphabet (white space included),
padding anywhere but in the last two places. Callers treat an empty result as "no usable data".
