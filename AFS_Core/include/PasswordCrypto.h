#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace AFS_Core {

namespace passwordcrypto {  // [PasswordCrypto.md#passwordcrypto]

std::string encrypt(const std::string& plaintext, const std::filesystem::path& key_file);

bool is_encrypted(const std::string& stored);  // [PasswordCrypto.md#is_encrypted]

std::optional<std::string> decrypt(const std::string& stored, const std::filesystem::path& key_file);  // [PasswordCrypto.md#decrypt]

} // namespace passwordcrypto

} // namespace AFS_Core
