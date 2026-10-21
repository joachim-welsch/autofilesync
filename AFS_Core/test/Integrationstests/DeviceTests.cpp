/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "Config.h"
#include "DeviceIdentity.h"
#include "DeviceLink.h"
#include "DevicePairing.h"
#include "FakePhone.h"
#include "ScopedLog.h"
#include "SshTest.h"
#include "StrUtil.h"
#include "SyncItem.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include <filesystem>
#include <fstream>
#include <libssh/libssh.h>

using namespace AFS_Core;
namespace fs = std::filesystem;

namespace {

const std::string PHONE_ID = "0123456789abcdef0123456789abcdef";
const std::string PHONE_NAME = "Pixel 8 Test";
const std::wstring ITEM_NAME = L"phone-item-test";

std::string local_sshd_host_key()  // [DeviceTests.md#local-sshd-key]
{
    ssh_session session = ssh_new();
    const unsigned int port = 22;
    ssh_options_set(session, SSH_OPTIONS_HOST, "127.0.0.1");
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    std::string line;
    if (ssh_connect(session) == SSH_OK) {
        ssh_key key = nullptr;
        if (ssh_get_server_publickey(session, &key) == SSH_OK) {
            char* base64 = nullptr;
            if (ssh_pki_export_pubkey_base64(key, &base64) == SSH_OK) {
                line = std::string(ssh_key_type_to_char(ssh_key_type(key))) + " " + base64;
                ssh_string_free_char(base64);
            }
            ssh_key_free(key);
        }
        ssh_disconnect(session);
    }
    ssh_free(session);
    return line;
}

std::string read_file(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

class DeviceLinkTest : public ::testing::Test {  // [DeviceTests.md#DeviceLinkTest]
protected:
    fs::path dir_;
    std::unique_ptr<tu::ScopedLog> log_;

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        dir_ = fs::temp_directory_path() / ("autofilesync_device_" + test_name + "_" + std::to_string(TestPaths::process_id()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        log_ = std::make_unique<tu::ScopedLog>(dir_ / "test.log");
        Config::instance().remove_device(PHONE_ID);
    }

    void TearDown() override
    {
        DeviceLink::instance().stop();
        Config::instance().erase_by_name(ITEM_NAME);
        Config::instance().remove_device(PHONE_ID);
        log_.reset();
        fs::remove_all(dir_);
    }

    DeviceLink& link()
    {
        DeviceLink& link = DeviceLink::instance();
        EXPECT_TRUE(link.start(dir_ / "pc", 0));
        return link;
    }

    tu::FakePhone phone(const std::string& name = "phone")
    {
        return tu::FakePhone(dir_ / name);
    }
};

} // namespace

TEST_F(DeviceLinkTest, IdentityIsCreatedOnceAndReloaded)  // [DeviceTests.md#identity]
{
    const auto first = DeviceIdentity::load_or_create(dir_ / "pc");
    const auto second = DeviceIdentity::load_or_create(dir_ / "pc");
    ASSERT_TRUE(first && second);
    EXPECT_EQ(first->fingerprint().size(), 64u);
    EXPECT_EQ(first->fingerprint(), second->fingerprint()) << "A second start must reuse the identity, or every phone would have to be paired again.";
    EXPECT_EQ(first->certificate_pem(), second->certificate_pem());
#ifndef _WIN32
    const auto permissions = fs::status(dir_ / "pc" / "device.key").permissions();
    EXPECT_EQ(permissions & (fs::perms::group_all | fs::perms::others_all), fs::perms::none) << "The private key must be readable by the owner only.";
#endif
}

TEST_F(DeviceLinkTest, LostKeyMakesANewIdentity)
{
    const auto first = DeviceIdentity::load_or_create(dir_ / "pc");
    fs::remove(dir_ / "pc" / "device.key");
    const auto second = DeviceIdentity::load_or_create(dir_ / "pc");
    ASSERT_TRUE(first && second);
    EXPECT_NE(first->fingerprint(), second->fingerprint());
}

TEST_F(DeviceLinkTest, PairingTokenIsSingleUse)  // [DeviceTests.md#token]
{
    DevicePairing pairing;
    EXPECT_FALSE(pairing.open());
    const std::string token = pairing.begin();
    EXPECT_EQ(token.size(), 32u);
    EXPECT_TRUE(pairing.open());
    EXPECT_FALSE(pairing.redeem("not the token"));
    EXPECT_TRUE(pairing.redeem(token));
    EXPECT_FALSE(pairing.redeem(token)) << "A token must work exactly once.";
    EXPECT_FALSE(pairing.open());
}

TEST_F(DeviceLinkTest, FiveWrongTokensCloseThePairing)
{
    DevicePairing pairing;
    const std::string token = pairing.begin();
    for (int i = 0; i < DevicePairing::FAILURE_LIMIT; ++i)
        EXPECT_FALSE(pairing.redeem("wrong"));
    EXPECT_FALSE(pairing.open());
    EXPECT_FALSE(pairing.redeem(token)) << "After the failure limit even the right token must be refused.";
}

TEST_F(DeviceLinkTest, QrContentCarriesTheOffer)  // [DeviceTests.md#qr]
{
    PairingOffer offer{"Mein PC", {"192.168.1.10", "10.0.0.2"}, 52381, "abc123", "deadbeef", 1700000000};
    const std::string content = qr_content(offer);
    EXPECT_EQ(content.rfind(std::string(PAIRING_URL) + "#v=1&n=Mein%20PC&a=192.168.1.10,10.0.0.2&p=52381&f=abc123&t=deadbeef&e=1700000000", 0), 0u) << content;
}

TEST_F(DeviceLinkTest, StartListensOnAFreePortAndIsIdempotent)
{
    DeviceLink& link = this->link();
    const uint16_t port = link.port();
    EXPECT_NE(port, 0);
    EXPECT_TRUE(link.start(dir_ / "pc", 0));
    EXPECT_EQ(link.port(), port);
    EXPECT_EQ(link.fingerprint().size(), 64u);
}

TEST_F(DeviceLinkTest, PairRegistersTheDevice)  // [DeviceTests.md#pair]
{
    DeviceLink& link = this->link();
    const PairingOffer offer = link.offer();
    EXPECT_EQ(offer.port, link.port());
    EXPECT_EQ(offer.fingerprint, link.fingerprint());
    const tu::FakePhone phone = this->phone();

    const auto reply = phone.send(link.port(), tu::FakePhone::pair_line(offer.token, PHONE_ID, PHONE_NAME));

    ASSERT_TRUE(reply.connected);
    EXPECT_EQ(reply.server_fingerprint, link.fingerprint()) << "The phone pins the fingerprint from the QR code; the server must present that certificate.";
    ASSERT_EQ(reply.type(), "paired") << (reply.lines.empty() ? std::string() : reply.lines[0]);
    EXPECT_EQ(reply.field("pc.fingerprint"), link.fingerprint());
    const auto device = Config::instance().find_device(PHONE_ID);
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->name, PHONE_NAME);
    EXPECT_EQ(device->cert_fingerprint, phone.fingerprint()) << "The fingerprint must come from the handshake, not from the message.";
    EXPECT_EQ(device->ssh_host_key.rfind("ecdsa-sha2-nistp256 ", 0), 0u);
    const auto paired = link.take_paired();
    ASSERT_TRUE(paired.has_value());
    EXPECT_EQ(paired->id, PHONE_ID);
    EXPECT_FALSE(link.take_paired().has_value()) << "take_paired hands the device over once.";
    const auto presence = link.presence(PHONE_ID);
    ASSERT_TRUE(presence.has_value());
    EXPECT_EQ(presence->address, "127.0.0.1");
    EXPECT_EQ(presence->sftp_port, 52382);
    EXPECT_FALSE(link.pairing().open()) << "The token is consumed by the pairing.";
}

TEST_F(DeviceLinkTest, PairWithWrongTokenIsRefused)
{
    DeviceLink& link = this->link();
    link.offer();
    const auto reply = phone().send(link.port(), tu::FakePhone::pair_line("ffffffffffffffffffffffffffffffff", PHONE_ID, PHONE_NAME));
    ASSERT_TRUE(reply.connected);
    EXPECT_EQ(reply.type(), "error");
    EXPECT_EQ(reply.field("reason"), "token_invalid");
    EXPECT_FALSE(Config::instance().find_device(PHONE_ID).has_value());
    EXPECT_TRUE(link.pairing().open()) << "One wrong token does not close the pairing.";
}

TEST_F(DeviceLinkTest, PairWithoutOpenDialogIsRefused)
{
    DeviceLink& link = this->link();
    const auto reply = phone().send(link.port(), tu::FakePhone::pair_line("0123456789abcdef0123456789abcdef", PHONE_ID, PHONE_NAME));
    ASSERT_TRUE(reply.connected);
    EXPECT_EQ(reply.field("reason"), "pairing_closed");
}

TEST_F(DeviceLinkTest, PairWithBadDeviceIdIsRefused)
{
    DeviceLink& link = this->link();
    const std::string token = link.offer().token;
    const auto reply = phone().send(link.port(), tu::FakePhone::pair_line(token, "not-hex", PHONE_NAME));
    EXPECT_EQ(reply.field("reason"), "bad_request");
    EXPECT_TRUE(link.pairing().open()) << "A malformed request must not consume the token.";
}

TEST_F(DeviceLinkTest, HelloFromPairedPhoneIsAnswered)  // [DeviceTests.md#hello]
{
    DeviceLink& link = this->link();
    const tu::FakePhone phone = this->phone();
    ASSERT_EQ(phone.send(link.port(), tu::FakePhone::pair_line(link.offer().token, PHONE_ID, PHONE_NAME)).type(), "paired");

    const auto reply = phone.send(link.port(), tu::FakePhone::hello_line(PHONE_ID));

    ASSERT_EQ(reply.lines.size(), 2u);
    EXPECT_EQ(reply.type(0), "items");
    EXPECT_EQ(reply.field("pc.name", 0), pc_name());
    EXPECT_EQ(reply.type(1), "idle");
}

TEST_F(DeviceLinkTest, HelloFromUnknownCertificateIsRefused)  // [DeviceTests.md#hello]
{
    DeviceLink& link = this->link();
    ASSERT_EQ(phone("first").send(link.port(), tu::FakePhone::pair_line(link.offer().token, PHONE_ID, PHONE_NAME)).type(), "paired");

    const auto reply = phone("second").send(link.port(), tu::FakePhone::hello_line(PHONE_ID));

    EXPECT_EQ(reply.type(), "error");
    EXPECT_EQ(reply.field("reason"), "unknown_device") << "Another certificate under a known id must be refused.";
}

TEST_F(DeviceLinkTest, HelloWithoutCertificateIsRefused)
{
    DeviceLink& link = this->link();
    const auto reply = phone().send(link.port(), tu::FakePhone::hello_line(PHONE_ID), false);
    ASSERT_TRUE(reply.connected);
    EXPECT_EQ(reply.field("reason"), "certificate_missing");
}

TEST_F(DeviceLinkTest, GarbageIsABadRequest)
{
    DeviceLink& link = this->link();
    EXPECT_EQ(phone().send(link.port(), "this is not json").field("reason"), "bad_request");
    EXPECT_EQ(phone().send(link.port(), "{\"type\":\"dance\"}").field("reason"), "bad_request");
}

TEST_F(DeviceLinkTest, RepairingReplacesTheEntry)  // [DeviceTests.md#repair]
{
    DeviceLink& link = this->link();
    ASSERT_EQ(phone("first").send(link.port(), tu::FakePhone::pair_line(link.offer().token, PHONE_ID, PHONE_NAME)).type(), "paired");
    const tu::FakePhone reinstalled = phone("second");

    ASSERT_EQ(reinstalled.send(link.port(), tu::FakePhone::pair_line(link.offer().token, PHONE_ID, "Pixel 8 neu")).type(), "paired");

    const Devices devices = Config::instance().devices_snapshot();
    EXPECT_EQ(std::ranges::count(devices, PHONE_ID, &Device::id), 1);
    EXPECT_EQ(Config::instance().find_device(PHONE_ID)->cert_fingerprint, reinstalled.fingerprint());
    EXPECT_EQ(reinstalled.send(link.port(), tu::FakePhone::hello_line(PHONE_ID)).type(), "items");
}

TEST_F(DeviceLinkTest, ConfigRoundTripsDevices)  // [DeviceTests.md#config]
{
    Device device{PHONE_ID, PHONE_NAME, std::string(64, 'a'), "ssh-ed25519 AAAA"};
    Config::instance().add(device);
    const fs::path file = dir_ / "config.json";
    ASSERT_TRUE(Config::instance().write(file.wstring()));
    Config::instance().remove_device(PHONE_ID);
    ASSERT_FALSE(Config::instance().find_device(PHONE_ID).has_value());

    ASSERT_TRUE(Config::instance().read(file.wstring()));

    const auto read = Config::instance().find_device(PHONE_ID);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, device);
}

TEST_F(DeviceLinkTest, HelloWithoutServerIsIdle)  // [DeviceTests.md#serve]
{
    DeviceLink& link = this->link();
    const tu::FakePhone phone = this->phone();
    ASSERT_EQ(phone.send(link.port(), tu::FakePhone::pair_line(link.offer().token, PHONE_ID, PHONE_NAME)).type(), "paired");
    const auto reply = phone.send(link.port(), tu::FakePhone::hello_line(PHONE_ID, {22, "u", "p", true}));
    ASSERT_EQ(reply.lines.size(), 2u);
    EXPECT_NE(reply.lines[0].find("\"items\":[]"), std::string::npos) << reply.lines[0];
    EXPECT_EQ(reply.type(1), "idle") << "Without items there is nothing to serve.";
    EXPECT_FALSE(link.serving(PHONE_ID));
}

TEST_F(DeviceLinkTest, HelloListsTheDeviceItems)  // [DeviceTests.md#serve]
{
    DeviceLink& link = this->link();
    const tu::FakePhone phone = this->phone();
    ASSERT_EQ(phone.send(link.port(), tu::FakePhone::pair_line(link.offer().token, PHONE_ID, PHONE_NAME)).type(), "paired");
    Config::instance().add(SyncItem(ITEM_NAME, SyncItemType::Dir, util::string_to_wstring(PHONE_ID) + L":/DCIM/Camera", (dir_ / "pc").wstring(), L"*"));
    const auto items = DeviceLink::items_of(PHONE_ID);
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].name, ITEM_NAME);
    EXPECT_EQ(items[0].direction, "toPc");

    const auto reply = phone.send(link.port(), tu::FakePhone::hello_line(PHONE_ID));

    ASSERT_EQ(reply.lines.size(), 2u);
    EXPECT_NE(reply.lines[0].find("\"name\":\"phone-item-test\",\"direction\":\"toPc\",\"enabled\":true"), std::string::npos) << reply.lines[0];
    EXPECT_EQ(reply.type(1), "idle") << "A hello without serving lists the items and asks for nothing.";
}

TEST_F(DeviceLinkTest, ServingPhoneGetsItsPass)  // [DeviceTests.md#serve]
{
    SKIP_WITHOUT_SSH_SERVER();
    const std::string host_key = local_sshd_host_key();
    ASSERT_FALSE(host_key.empty()) << "The local sshd's host key could not be read.";
    DeviceLink& link = this->link();
    const tu::FakePhone phone = this->phone();
    ASSERT_EQ(phone.send(link.port(), tu::FakePhone::pair_line(link.offer().token, PHONE_ID, PHONE_NAME, host_key)).type(), "paired");
    const fs::path source = dir_ / "phone-side";
    const fs::path target = dir_ / "pc-side";
    fs::create_directories(source);
    fs::create_directories(target);
    std::ofstream(source / "IMG_0001.jpg") << std::string(200000, 'j');
    std::ofstream(source / "IMG_0002.jpg") << "second";
    Config::instance().add(SyncItem(ITEM_NAME, SyncItemType::Dir, util::string_to_wstring(PHONE_ID) + L":" + source.wstring(), target.wstring(), L"*"));

    const auto reply = phone.send(link.port(), tu::FakePhone::hello_line(PHONE_ID, {22, SSHTest::SSH_USER, SSHTest::SSH_PASSWORD, true}));

    ASSERT_GE(reply.lines.size(), 3u) << "items, serve and done at least";
    EXPECT_EQ(reply.type(0), "items");
    EXPECT_EQ(reply.type(1), "serve");
    const size_t last = reply.lines.size() - 1;
    EXPECT_EQ(reply.type(last), "done") << reply.lines[last];
    EXPECT_EQ(reply.field("completed", last), "true");
    EXPECT_EQ(reply.field("errors", last), "0");
    EXPECT_EQ(read_file(target / "IMG_0001.jpg"), std::string(200000, 'j')) << "The pass did not copy the phone's file.";
    EXPECT_EQ(read_file(target / "IMG_0002.jpg"), "second");
    EXPECT_FALSE(link.serving(PHONE_ID)) << "Serving ends with done.";
    const auto account = Config::instance().device_account(PHONE_ID);
    ASSERT_TRUE(account.has_value());
    EXPECT_TRUE(account->host().empty()) << "The live account must be gone after the pass.";
    EXPECT_TRUE(account->password().empty());
    EXPECT_FALSE(Synchronize::has_instance()) << "The run the link created must be destroyed.";
}

TEST_F(DeviceLinkTest, WrongHostKeyStopsThePass)  // [DeviceTests.md#serve]
{
    SKIP_WITHOUT_SSH_SERVER();
    DeviceLink& link = this->link();
    const tu::FakePhone phone = this->phone();
    ASSERT_EQ(phone.send(link.port(), tu::FakePhone::pair_line(link.offer().token, PHONE_ID, PHONE_NAME, "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")).type(), "paired");
    const fs::path source = dir_ / "phone-side";
    const fs::path target = dir_ / "pc-side";
    fs::create_directories(source);
    fs::create_directories(target);
    std::ofstream(source / "secret.txt") << "must not arrive";
    Config::instance().add(SyncItem(ITEM_NAME, SyncItemType::Dir, util::string_to_wstring(PHONE_ID) + L":" + source.wstring(), target.wstring(), L"*"));

    const auto reply = phone.send(link.port(), tu::FakePhone::hello_line(PHONE_ID, {22, SSHTest::SSH_USER, SSHTest::SSH_PASSWORD, true}));

    EXPECT_EQ(reply.type(reply.lines.size() - 1), "done");
    EXPECT_FALSE(fs::exists(target / "secret.txt")) << "A server with another host key must not be used.";
}
