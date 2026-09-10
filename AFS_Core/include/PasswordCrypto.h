#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace AFS_Core {

// Encrypts secrets (SSH passwords) at rest so they no longer sit in plaintext
// inside the JSON config file - protects against casual disclosure (support
// files, backups, accidental paste of the config) but not against an attacker
// with full read access to the user account, since the key lives next to the
// config in a file only readable by the owner.
namespace passwordcrypto {

std::string encrypt(const std::string& plaintext, const std::filesystem::path& key_file);

// True if `stored` carries our encrypted-format marker. Callers must check
// this before falling back to `stored` itself as a legacy plaintext password:
// decrypt() returning nullopt does not by itself mean "not encrypted" - see
// its own comment below.
bool is_encrypted(const std::string& stored);

// Returns std::nullopt if `stored` is not in our encrypted format (legacy
// plaintext from before this feature) or if decryption/authentication fails.
// These two cases are NOT the same thing and must not be treated alike by a
// caller: falling back to `stored` itself as "the password" when is_encrypted()
// is true but decrypt() failed uses the raw ciphertext blob (e.g.
// "$AES256GCM$...") as the literal password instead of surfacing that
// decryption failed (e.g. the encryption key was lost or changed).
std::optional<std::string> decrypt(const std::string& stored, const std::filesystem::path& key_file);

} // namespace passwordcrypto

} // namespace AFS_Core
