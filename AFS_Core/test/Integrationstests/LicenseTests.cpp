/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#include "pch.h"
#include "ScopedLog.h"
#include <License.h>
#include <LicensePublicKey.h>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace AFS_Core;
using namespace AFS_Core::license;

namespace fs = std::filesystem;

namespace {

class SignedLicenseFile {  // [LicenseTests.md#SignedLicenseFile]
public:
    explicit SignedLicenseFile(const std::string& payload)
        : path_(fs::temp_directory_path() / "afs_license_test" / LICENSE_FILE_NAME)
    {
        fs::create_directories(path_.parent_path());
        const std::span<const unsigned char> bytes(reinterpret_cast<const unsigned char*>(payload.data()), payload.size());
        signature_ = sign_for_test(bytes, public_key_);
        std::ofstream(path_, std::ios::binary) << payload << "Signature: " << signature_ << "\n";
    }
    ~SignedLicenseFile()
    {
        std::error_code ignored;
        fs::remove_all(path_.parent_path(), ignored);
    }
    const fs::path& path() const { return path_; }
    const fs::path directory() const { return path_.parent_path(); }
    PublicKey public_key() const { return public_key_; }
    void overwrite(const std::string& content) const { std::ofstream(path_, std::ios::binary) << content; }
    const std::string& signature() const { return signature_; }
private:
    fs::path path_;
    std::array<unsigned char, PUBLIC_KEY_SIZE> public_key_{};
    std::string signature_;
};

std::string file_content(const fs::path& path)
{
    std::ostringstream content;
    content << std::ifstream(path, std::ios::binary).rdbuf();
    return content.str();
}

}

TEST(LicenseTests, ValidFileNamesTheLicensee)
{
    const tu::ScopedLog log(fs::temp_directory_path() / "afs_license_test.log");
    const SignedLicenseFile file("Licensee: Firma Beispiel GmbH\n");

    const std::optional<License> license = read(file.path(), file.public_key());

    ASSERT_TRUE(license.has_value());
    EXPECT_EQ(license->licensee, L"Firma Beispiel GmbH");
}

TEST(LicenseTests, WindowsLineEndingsVerifyToo)  // [LicenseTests.md#line-endings]
{
    const tu::ScopedLog log(fs::temp_directory_path() / "afs_license_test.log");
    const SignedLicenseFile file("Licensee: Firma Beispiel GmbH\nProduct: AutoFileSync\n");
    std::string content = file_content(file.path());
    for (std::size_t pos = content.find('\n'); pos != std::string::npos; pos = content.find('\n', pos + 2))
        content.insert(pos, "\r");
    file.overwrite(content);

    const std::optional<License> license = read(file.path(), file.public_key());

    ASSERT_TRUE(license.has_value());
    EXPECT_EQ(license->licensee, L"Firma Beispiel GmbH");
}

TEST(LicenseTests, TamperedLicenseeIsRejectedAndLogged)
{
    const tu::ScopedLog log(fs::temp_directory_path() / "afs_license_test.log", LogLevel::Warning);
    const SignedLicenseFile file("Licensee: Firma Beispiel GmbH\n");
    file.overwrite("Licensee: Jemand anderes\nSignature: " + file.signature() + "\n");

    EXPECT_FALSE(read(file.path(), file.public_key()).has_value());
    EXPECT_NE(file_content(log.path()).find(file.path().string()), std::string::npos) << "Die Warnung nennt die Datei nicht.";
}

TEST(LicenseTests, WrongKeyMissingSignatureAndMissingFileAreRejected)
{
    const tu::ScopedLog log(fs::temp_directory_path() / "afs_license_test.log");
    const SignedLicenseFile file("Licensee: Firma Beispiel GmbH\n");
    const std::array<unsigned char, PUBLIC_KEY_SIZE> other_key{};
    EXPECT_FALSE(read(file.path(), other_key).has_value());

    file.overwrite("Licensee: Firma Beispiel GmbH\n");
    EXPECT_FALSE(read(file.path(), file.public_key()).has_value());

    EXPECT_FALSE(read(file.directory() / "fehlt.lic", file.public_key()).has_value());
}

TEST(LicenseTests, FindTakesTheFirstDirectoryWithAFile)
{
    const tu::ScopedLog log(fs::temp_directory_path() / "afs_license_test.log");
    const SignedLicenseFile file("Licensee: Firma Beispiel GmbH\n");
    const fs::path empty = fs::temp_directory_path() / "afs_license_test_empty";
    fs::create_directories(empty);

    const std::optional<License> license = find({empty, file.directory()}, file.public_key());

    ASSERT_TRUE(license.has_value());
    EXPECT_EQ(license->licensee, L"Firma Beispiel GmbH");
    EXPECT_FALSE(find({empty}, file.public_key()).has_value());
    fs::remove_all(empty);
}

TEST(LicenseTests, ProductKeyVerifiesAnIssuedLicense)  // [LicenseTests.md#issued-license]
{
    const tu::ScopedLog log(fs::temp_directory_path() / "afs_license_test.log");
    const fs::path file = fs::temp_directory_path() / "afs_license_issued.lic";
    std::ofstream(file, std::ios::binary)
        << "Licensee: Firma Beispiel GmbH\n"
        << "Signature: 75921b43c5513d14a8235f3189817bcea1b9608682df49e726e81b6d9a290d176f414fc7f4a97f038753ad05509214996907a15702ca8a69ec32becaf8133606\n";

    const std::optional<License> license = read(file, PRODUCT_PUBLIC_KEY);

    fs::remove(file);
    ASSERT_TRUE(license.has_value()) << "Der eingebaute Schluessel passt nicht zu tools/makeLicense.sh.";
    EXPECT_EQ(license->licensee, L"Firma Beispiel GmbH");
}

TEST(LicenseTests, EmptyLicenseeIsAValidPrivateUseLicense)  // [LicenseTests.md#private-use]
{
    const tu::ScopedLog log(fs::temp_directory_path() / "afs_license_test.log");
    const SignedLicenseFile file("Licensee: \n");

    const std::optional<License> license = read(file.path(), file.public_key());

    ASSERT_TRUE(license.has_value());
    EXPECT_TRUE(license->private_use());
    EXPECT_TRUE(license->licensee.empty());
}
