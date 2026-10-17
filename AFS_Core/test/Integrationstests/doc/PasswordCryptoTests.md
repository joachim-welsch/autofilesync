# PasswordCryptoTests

Module comment for PasswordCryptoTests.cpp.

## windows-has-no-posix-bits

Windows has no POSIX permission bits: std::filesystem synthesises them and always reports full
access for group and others, regardless of the actual ACL. The assertion is therefore not
verifiable here - and passwordcrypto sets no restricting ACL on Windows either, the key file
merely lives in the user profile.

## decrypt-after-reload

Decrypting in a "new process" (same key file, freshly loaded) must still work.

## RoundTripsWithinProcessEvenWhenKeyPersistenceFails

Fehler 6 (round 5): load_or_create_key() re-read/re-generated the key from disk on every call
with no in-memory cache. If persisting a freshly generated key failed (unwritable directory),
every subsequent call - including the very next decrypt() of what the preceding encrypt() just
produced, in the same process - got a *different* random key, so decryption failed even without
an actual restart. The key is now cached in memory per key-file path so a run stays internally
consistent regardless of whether the on-disk key could be persisted.

## blocked-key-parent

A key file whose parent is a regular file, not a directory: creating the parent and opening the
key file for writing can never succeed.

## second-round-trip

A second, independent encrypt/decrypt pair must also round-trip - proves the cached key is
reused, not just "got lucky" on the first call.

## IsEncryptedRecognisesOnlyTheMarker

The marker decides whether Config treats a stored password as ciphertext or as a legacy plain
text to be encrypted on the next save; a plain text that merely contains the letters must stay
plain text.
