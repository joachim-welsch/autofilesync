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

    EXPECT_NE(first, second);  // a fresh random nonce per call is what makes GCM safe to reuse a key with
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
    GTEST_SKIP() << "POSIX-Rechtebits gibt es unter Windows nicht";  // [PasswordCryptoTests.md#windows-has-no-posix-bits]
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

    const auto decrypted = passwordcrypto::decrypt(encrypted, key_file_);  // [PasswordCryptoTests.md#decrypt-after-reload]

    EXPECT_EQ(decrypted, "correct horse battery staple");
}

TEST(PasswordCryptoTests, RoundTripsWithinProcessEvenWhenKeyPersistenceFails)  // [PasswordCryptoTests.md#RoundTripsWithinProcessEvenWhenKeyPersistenceFails]
{
    const fs::path blocker = fs::temp_directory_path() / ("afx_pwcrypto_blocker_" + std::to_string(getpid()));  // [PasswordCryptoTests.md#blocked-key-parent]
    { std::ofstream(blocker) << "not a directory"; }
    const fs::path key_file = blocker / "secret.key";

    const std::string plaintext = "s3cr3t-test-password";
    const std::string ciphertext = passwordcrypto::encrypt(plaintext, key_file);
    auto decrypted = passwordcrypto::decrypt(ciphertext, key_file);
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);

    const std::string ciphertext2 = passwordcrypto::encrypt(plaintext, key_file);  // [PasswordCryptoTests.md#second-round-trip]
    auto decrypted2 = passwordcrypto::decrypt(ciphertext2, key_file);
    ASSERT_TRUE(decrypted2.has_value());
    EXPECT_EQ(*decrypted2, plaintext);

    fs::remove(blocker);
}
