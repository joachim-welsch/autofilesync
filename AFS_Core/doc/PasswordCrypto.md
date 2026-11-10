# PasswordCrypto

Module comment for PasswordCrypto.h / PasswordCrypto.cpp.

## passwordcrypto

Encrypts secrets (SSH passwords) at rest so they no longer sit in plaintext inside the JSON
config file - protects against casual disclosure (support files, backups, accidental paste of
the config) but not against an attacker with full read access to the user account, since the key
lives next to the config in a file only readable by the owner. On Windows the key file is sealed
for the user in addition (SecretBlob.md#secretblob), so that a copy of the directory does not
carry a usable key.

## is_encrypted

True if `stored` carries our encrypted-format marker. Callers must check this before falling
back to `stored` itself as a legacy plaintext password: decrypt() returning nullopt does not by
itself mean "not encrypted" - see its own comment below.

## decrypt

Returns std::nullopt if `stored` is not in our encrypted format (legacy plaintext from before
this feature) or if decryption/authentication fails. These two cases are NOT the same thing and
must not be treated alike by a caller: falling back to `stored` itself as "the password" when
is_encrypted() is true but decrypt() failed uses the raw ciphertext blob (e.g. "$AES256GCM$...")
as the literal password instead of surfacing that decryption failed (e.g. the encryption key was
lost or changed).

## key-cache

Generates a random key on first use and stores it next to the config with owner-only
permissions, like an SSH private key; reused afterward so previously encrypted passwords stay
decryptable across runs.

Cached in memory per key file path: load_or_create_key() re-reads from disk on every call, so if
persisting a freshly generated key fails (read-only filesystem, missing directory), an uncached
implementation would hand out a *different* random key to every subsequent call - even the very
next decrypt() of what the preceding encrypt() just produced would fail. The cache keeps a run
internally consistent; only surviving an actual restart still depends on the write below
succeeding, which is why that failure is logged.

## key-file-permissions

Written through Util.md#private-files, so the key is owner-only (mode 0600) from the moment the
file exists.

## unusable-key

A key file that exists but cannot be read - locked by another program for a moment, sealed for
another user, damaged - is not a missing one. Generating a new key in its place would overwrite
the only key that opens the stored passwords. The call fails instead and is not cached, so the
next one tries again; only a file that is absent or empty leads to a new key.

## random-failure

RAND_bytes() reports when OpenSSL has no seeded generator. A key that stayed at its zero
initialisation would protect nothing and, once persisted, would do so for good; a repeated
nonce under the same key breaks GCM's confidentiality and authenticity for both messages. So
either failure refuses to encrypt or decrypt: encrypt() returns an empty string, which the
configuration writes as "no password", and the log says why.

## key-file-replaced-whole

On Windows the key is written beside the key file and renamed over it: a crash in between leaves
either the old key or the new one, never a truncated file that the next start could not read.
