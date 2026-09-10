#include "pch.h"
#include "PasswordCrypto.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {

class PasswordCryptoTest : public ::testing::Test {
protected:
    fs::path test_dir_;
    fs::path key_file_;

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        test_dir_ = fs::temp_directory_path() / ("autofilesync_pwcrypto_" + test_name + "_" + std::to_string(getpid()));
        fs::remove_all(test_dir_);
        fs::create_directories(test_dir_);
        key_file_ = test_dir_ / "secret.key";
    }

    void TearDown() override
    {
        fs::remove_all(test_dir_);
    }
};

} // namespace

TEST_F(PasswordCryptoTest, RoundTripsPlaintext)
{
    const std::string plaintext = "correct horse battery staple";

    const std::string encrypted = passwordcrypto::encrypt(plaintext, key_file_);
    const auto decrypted = passwordcrypto::decrypt(encrypted, key_file_);

    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);
}

TEST_F(PasswordCryptoTest, EncryptedValueDoesNotContainPlaintext)
{
    const std::string plaintext = "correct horse battery staple";

    const std::string encrypted = passwordcrypto::encrypt(plaintext, key_file_);

    EXPECT_EQ(encrypted.find(plaintext), std::string::npos);
}

TEST_F(PasswordCryptoTest, SameInputEncryptsDifferentlyEachTime)
{
    const std::string plaintext = "correct horse battery staple";

    const std::string first = passwordcrypto::encrypt(plaintext, key_file_);
    const std::string second = passwordcrypto::encrypt(plaintext, key_file_);

    // A fresh random nonce per call is what makes GCM safe to reuse a key with.
    EXPECT_NE(first, second);
    EXPECT_EQ(passwordcrypto::decrypt(first, key_file_), plaintext);
    EXPECT_EQ(passwordcrypto::decrypt(second, key_file_), plaintext);
}

TEST_F(PasswordCryptoTest, RejectsTamperedCiphertext)
{
    std::string encrypted = passwordcrypto::encrypt("correct horse battery staple", key_file_);
    encrypted.back() = (encrypted.back() == 'A') ? 'B' : 'A'; // flip a base64 char near the auth tag

    const auto decrypted = passwordcrypto::decrypt(encrypted, key_file_);

    EXPECT_FALSE(decrypted.has_value());
}

TEST_F(PasswordCryptoTest, RejectsCiphertextEncryptedWithADifferentKey)
{
    const fs::path other_key_file = test_dir_ / "other.key";
    const std::string encrypted = passwordcrypto::encrypt("correct horse battery staple", key_file_);

    const auto decrypted = passwordcrypto::decrypt(encrypted, other_key_file);

    EXPECT_FALSE(decrypted.has_value());
}

TEST_F(PasswordCryptoTest, TreatsUnmarkedStringAsLegacyPlaintext)
{
    const auto decrypted = passwordcrypto::decrypt("some-old-plaintext-password", key_file_);

    EXPECT_FALSE(decrypted.has_value());
}

TEST_F(PasswordCryptoTest, KeyFileIsCreatedWithOwnerOnlyPermissions)
{
    passwordcrypto::encrypt("correct horse battery staple", key_file_);

    ASSERT_TRUE(fs::exists(key_file_));
#ifdef _WIN32
    // Windows kennt die POSIX-Rechtebits nicht: std::filesystem synthetisiert
    // sie und meldet immer Vollzugriff fuer Gruppe und Andere, unabhaengig von
    // der tatsaechlichen ACL. Die Aussage ist hier also nicht pruefbar - und
    // passwordcrypto setzt auf Windows auch keine einschraenkende ACL, die
    // Schluesseldatei liegt nur im Benutzerprofil.
    GTEST_SKIP() << "POSIX-Rechtebits gibt es unter Windows nicht";
#else
    const auto perms = fs::status(key_file_).permissions();
    EXPECT_EQ(perms & fs::perms::group_all, fs::perms::none);
    EXPECT_EQ(perms & fs::perms::others_all, fs::perms::none);
#endif
}

TEST_F(PasswordCryptoTest, ReusesPersistedKeyAcrossCalls)
{
    const std::string encrypted = passwordcrypto::encrypt("correct horse battery staple", key_file_);
    ASSERT_TRUE(fs::exists(key_file_));

    // Decrypting in a "new process" (same key file, freshly loaded) must still work.
    const auto decrypted = passwordcrypto::decrypt(encrypted, key_file_);

    EXPECT_EQ(decrypted, "correct horse battery staple");
}

// Fehler 6 (round 5): load_or_create_key() re-read/re-generated the key from
// disk on every call with no in-memory cache. If persisting a freshly
// generated key failed (unwritable directory), every subsequent call -
// including the very next decrypt() of what the preceding encrypt() just
// produced, in the same process - got a *different* random key, so
// decryption failed even without an actual restart. The key is now cached in
// memory per key-file path so a run stays internally consistent regardless
// of whether the on-disk key could be persisted.
TEST(PasswordCryptoTests, RoundTripsWithinProcessEvenWhenKeyPersistenceFails)
{
    // A key file whose parent is a regular file, not a directory: creating
    // the parent and opening the key file for writing can never succeed.
    const fs::path blocker = fs::temp_directory_path() / ("afx_pwcrypto_blocker_" + std::to_string(getpid()));
    { std::ofstream(blocker) << "not a directory"; }
    const fs::path key_file = blocker / "secret.key";

    const std::string plaintext = "s3cr3t-test-password";
    const std::string ciphertext = passwordcrypto::encrypt(plaintext, key_file);
    auto decrypted = passwordcrypto::decrypt(ciphertext, key_file);
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);

    // A second, independent encrypt/decrypt pair must also round-trip -
    // proves the cached key is reused, not just "got lucky" on the first call.
    const std::string ciphertext2 = passwordcrypto::encrypt(plaintext, key_file);
    auto decrypted2 = passwordcrypto::decrypt(ciphertext2, key_file);
    ASSERT_TRUE(decrypted2.has_value());
    EXPECT_EQ(*decrypted2, plaintext);

    fs::remove(blocker);
}
