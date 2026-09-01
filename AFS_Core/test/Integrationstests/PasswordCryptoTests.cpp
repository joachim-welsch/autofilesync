#include "pch.h"
#include "PasswordCrypto.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace {

class PasswordCryptoTest : public ::testing::Test {
protected:
    fs::path testDir_;
    fs::path keyFile_;

    void SetUp() override
    {
        const std::string testName = testing::UnitTest::GetInstance()->current_test_info()->name();
        testDir_ = fs::temp_directory_path() / ("autofilesync_pwcrypto_" + testName + "_" + std::to_string(getpid()));
        fs::remove_all(testDir_);
        fs::create_directories(testDir_);
        keyFile_ = testDir_ / "secret.key";
    }

    void TearDown() override
    {
        fs::remove_all(testDir_);
    }
};

} // namespace

TEST_F(PasswordCryptoTest, RoundTripsPlaintext)
{
    const std::string plaintext = "correct horse battery staple";

    const std::string encrypted = passwordcrypto::encrypt(plaintext, keyFile_);
    const auto decrypted = passwordcrypto::decrypt(encrypted, keyFile_);

    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);
}

TEST_F(PasswordCryptoTest, EncryptedValueDoesNotContainPlaintext)
{
    const std::string plaintext = "correct horse battery staple";

    const std::string encrypted = passwordcrypto::encrypt(plaintext, keyFile_);

    EXPECT_EQ(encrypted.find(plaintext), std::string::npos);
}

TEST_F(PasswordCryptoTest, SameInputEncryptsDifferentlyEachTime)
{
    const std::string plaintext = "correct horse battery staple";

    const std::string first = passwordcrypto::encrypt(plaintext, keyFile_);
    const std::string second = passwordcrypto::encrypt(plaintext, keyFile_);

    // A fresh random nonce per call is what makes GCM safe to reuse a key with.
    EXPECT_NE(first, second);
    EXPECT_EQ(passwordcrypto::decrypt(first, keyFile_), plaintext);
    EXPECT_EQ(passwordcrypto::decrypt(second, keyFile_), plaintext);
}

TEST_F(PasswordCryptoTest, RejectsTamperedCiphertext)
{
    std::string encrypted = passwordcrypto::encrypt("correct horse battery staple", keyFile_);
    encrypted.back() = (encrypted.back() == 'A') ? 'B' : 'A'; // flip a base64 char near the auth tag

    const auto decrypted = passwordcrypto::decrypt(encrypted, keyFile_);

    EXPECT_FALSE(decrypted.has_value());
}

TEST_F(PasswordCryptoTest, RejectsCiphertextEncryptedWithADifferentKey)
{
    const fs::path otherKeyFile = testDir_ / "other.key";
    const std::string encrypted = passwordcrypto::encrypt("correct horse battery staple", keyFile_);

    const auto decrypted = passwordcrypto::decrypt(encrypted, otherKeyFile);

    EXPECT_FALSE(decrypted.has_value());
}

TEST_F(PasswordCryptoTest, TreatsUnmarkedStringAsLegacyPlaintext)
{
    const auto decrypted = passwordcrypto::decrypt("some-old-plaintext-password", keyFile_);

    EXPECT_FALSE(decrypted.has_value());
}

TEST_F(PasswordCryptoTest, KeyFileIsCreatedWithOwnerOnlyPermissions)
{
    passwordcrypto::encrypt("correct horse battery staple", keyFile_);

    ASSERT_TRUE(fs::exists(keyFile_));
#ifdef _WIN32
    // Windows kennt die POSIX-Rechtebits nicht: std::filesystem synthetisiert
    // sie und meldet immer Vollzugriff fuer Gruppe und Andere, unabhaengig von
    // der tatsaechlichen ACL. Die Aussage ist hier also nicht pruefbar - und
    // passwordcrypto setzt auf Windows auch keine einschraenkende ACL, die
    // Schluesseldatei liegt nur im Benutzerprofil.
    GTEST_SKIP() << "POSIX-Rechtebits gibt es unter Windows nicht";
#else
    const auto perms = fs::status(keyFile_).permissions();
    EXPECT_EQ(perms & fs::perms::group_all, fs::perms::none);
    EXPECT_EQ(perms & fs::perms::others_all, fs::perms::none);
#endif
}

TEST_F(PasswordCryptoTest, ReusesPersistedKeyAcrossCalls)
{
    const std::string encrypted = passwordcrypto::encrypt("correct horse battery staple", keyFile_);
    ASSERT_TRUE(fs::exists(keyFile_));

    // Decrypting in a "new process" (same key file, freshly loaded) must still work.
    const auto decrypted = passwordcrypto::decrypt(encrypted, keyFile_);

    EXPECT_EQ(decrypted, "correct horse battery staple");
}

// Fehler 6 (round 5): loadOrCreateKey() re-read/re-generated the key from
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
    const fs::path keyFile = blocker / "secret.key";

    const std::string plaintext = "s3cr3t-test-password";
    const std::string ciphertext = passwordcrypto::encrypt(plaintext, keyFile);
    auto decrypted = passwordcrypto::decrypt(ciphertext, keyFile);
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);

    // A second, independent encrypt/decrypt pair must also round-trip -
    // proves the cached key is reused, not just "got lucky" on the first call.
    const std::string ciphertext2 = passwordcrypto::encrypt(plaintext, keyFile);
    auto decrypted2 = passwordcrypto::decrypt(ciphertext2, keyFile);
    ASSERT_TRUE(decrypted2.has_value());
    EXPECT_EQ(*decrypted2, plaintext);

    fs::remove(blocker);
}
