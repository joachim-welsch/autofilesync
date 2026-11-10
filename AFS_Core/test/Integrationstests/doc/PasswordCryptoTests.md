# PasswordCryptoTests

Module comment for PasswordCryptoTests.cpp.

## windows-has-no-posix-bits

Windows has no POSIX permission bits: std::filesystem synthesises them and always reports full
access for group and others, regardless of the actual ACL. There the protection is the seal of
SecretBlob.md#secretblob, so the test checks that the file does not hold the raw key and that
its content is sealed.

## UnreadableKeyFileIsNotReplaced

A key file with content that is no key stands for every file that is there but cannot be used.
The old code generated a new key over it, which made every stored password undecryptable for
good. Nothing may be encrypted, the file must stay as it is, and - second test - the failure
must not stick once the file is readable again, because a file locked by a virus scanner is
readable a moment later.

## KeyFileOfAnOlderVersionKeepsItsKey

A raw 32-byte file is what versions before the seal wrote. It must keep working and carry the
same key afterwards, or an update would cost every stored password; on Windows it is sealed in
passing, on the other platforms outdated() is never true.

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
