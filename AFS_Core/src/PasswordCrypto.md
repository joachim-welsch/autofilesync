# PasswordCrypto

Module comment for PasswordCrypto.h / PasswordCrypto.cpp.

## passwordcrypto

Encrypts secrets (SSH passwords) at rest so they no longer sit in plaintext inside the JSON
config file - protects against casual disclosure (support files, backups, accidental paste of
the config) but not against an attacker with full read access to the user account, since the key
lives next to the config in a file only readable by the owner.

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

O_EXCL makes the file owner-only (mode 0600) from the moment it exists, avoiding the window a
separate chmod() call after creation would leave.
