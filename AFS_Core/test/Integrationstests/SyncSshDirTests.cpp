#include "pch.h"
#include "SyncDir.h"
#include "Synchronize.h"
#include "DirUtil.h"
#include "SyncSshDirTests.h"
#include "Config.h"
#include "SshFileInspector.h"
#include "SshAccount.h"
#include <Logging.h>
#include <filesystem>
#include <fstream>
#include "SshTest.h"
#include "FileUtil.h"

using namespace AFS_Core;

using namespace SSHTest;
using namespace tu;

// Test-Konfiguration - Anpassen an Ihre SSH-Umgebung
namespace SyncSshDirTestConfig {
    const std::string SRC_TEST_DIR = tu::concat_path(tu::test_temp_dir(), "test_ssh_dir_src");
	const std::string DEST_TEST_DIR = tu::concat_path(tu::test_temp_dir(), "test_ssh_dir_dest");
    
    // Hilfsfunktion um ein SshAccount-Objekt zu erstellen
    inline SshAccount create_test_account(const std::string& user = SSH_USER,
                                        const std::string& host = SSH_HOST,
                                        const std::string& password = SSH_PASSWORD) {
        SshAccount acc;
        acc.set_name("test_dir_account");
        acc.set_user(user);
        acc.set_host(host);
        acc.set_password(password);
        return acc;
    }
}

// Fixture für SSH-Dir-Tests
class SyncSshDirTest : public ::testing::Test {
protected:
    void SetUp() override {
        namespace fs = std::filesystem;

        SKIP_WITHOUT_SSH_SERVER();

        // Lokales Testverzeichnis erstellen und aufräumen
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
        fs::create_directories(SyncSshDirTestConfig::SRC_TEST_DIR);
        
        // Account zur Config hinzufügen
        SshAccount acc;
        acc.set_name("localhost");
        acc.set_user(SSH_USER);
        acc.set_host(SSH_HOST);
        acc.set_password(SSH_PASSWORD);
        Config::instance().add(acc);

        // Remote-Zielverzeichnisse aus frueheren Laeufen entfernen: sie
        // gehoeren dem SSH-Testkonto, das lokale fs::remove_all() oben kann
        // sie nicht anfassen - ein stehen gebliebenes Ziel wuerde als
        // "bereits synchronisiert" durchgehen und copyCount faelschlich
        // bei 0 belassen. Jeder Aufruf verbindet neu per SSH, deshalb nur
        // einmal pro Prozess statt vor jedem einzelnen Test.
        static bool ssh_dest_cleaned = false;
        if (!ssh_dest_cleaned) {
            tu::erase_ssh_test_dir_variants(
                L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()),
                { L"_empty", L"_many", L"_mixed", L"_overwrite" });
            ssh_dest_cleaned = true;
        }

        // Synchronize zurücksetzen - ohne dies bleibt stop_ aus einem früheren
        // Test stehen und SyncFile::operator()() tut kommentarlos nichts.
        Synchronize::start();
        }

    void TearDown() override {
        namespace fs = std::filesystem;
        
        // Lokales Testverzeichnis aufräumen
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
    }
};

// Test 1: Lokales Verzeichnis mit mehreren Dateien nach SSH kopieren
// Hinweis: SyncDir kopiert nur Dateien im angegebenen Verzeichnis, keine Unterverzeichnisse
TEST_F(SyncSshDirTest, SyncLocalDirToRemote)
{
    namespace fs = std::filesystem;
    
    // Lokale Testdateien erstellen (nur im Hauptverzeichnis)
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR;
    std::ofstream(tu::concat_path(local_dir, "file1.txt")) << "content1";
    std::ofstream(tu::concat_path(local_dir, "file2.txt")) << "content2";
    std::ofstream(tu::concat_path(local_dir, "file3.txt")) << "content3";
    
    // SyncItem konfigurieren
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    
    // Prüfen ob Dateien auf Remote existieren
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo info1 = inspector.inspect_file(SyncSshDirTestConfig::DEST_TEST_DIR + "/file1.txt");
    RemoteFileInfo info2 = inspector.inspect_file(SyncSshDirTestConfig::DEST_TEST_DIR + "/file2.txt");
    RemoteFileInfo info3 = inspector.inspect_file(SyncSshDirTestConfig::DEST_TEST_DIR + "/file3.txt");
    
    EXPECT_TRUE(info1.exists) << "file1.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(info2.exists) << "file2.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(info3.exists) << "file3.txt wurde nicht auf Remote gefunden";
    
    EXPECT_GT(info1.size_bytes, 0);
    EXPECT_GT(info2.size_bytes, 0);
    EXPECT_GT(info3.size_bytes, 0);
}

// Test 2: Remote-Verzeichnis nach lokal synchronisieren
TEST_F(SyncSshDirTest, SyncRemoteDirToLocal)
{
    namespace fs = std::filesystem;

    // Remote-Testverzeichnis vorbereiten (per Sync von lokal nach remote)
    fs::create_directories(SyncSshDirTestConfig::SRC_TEST_DIR);
    fs::create_directories(SyncSshDirTestConfig::DEST_TEST_DIR);

    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "sshd_config")) << "test";
    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "other.txt")) << "other";

    {
        std::wstring seed_src_path = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(),
            SyncSshDirTestConfig::SRC_TEST_DIR.end()) + L"/";
        std::wstring seed_dest_path = L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(),
            SyncSshDirTestConfig::DEST_TEST_DIR.end());

        SyncItem seed;
        seed.sync_cycle(0);
        seed.sync_item_type(SyncItemType::Dir);
        seed.source(seed_src_path);
        seed.target(seed_dest_path);

        std::size_t seed_copy_count = 0;
        SyncDir seed_sync(seed, seed_copy_count, false);
        seed_sync();
    }

    // Jetzt Remote -> Local synchronisieren
    std::wstring src_path = L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(),
        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    std::wstring dest_path = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(),
        SyncSshDirTestConfig::SRC_TEST_DIR.end()) + L"/ssh_copy";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();

    // Prüfen ob lokales Verzeichnis erstellt wurde
    EXPECT_TRUE(fs::exists(SyncSshDirTestConfig::SRC_TEST_DIR + "/ssh_copy"));

    // Prüfen ob Dateien kopiert wurden (z.B. sshd_config sollte existieren)
    std::string expected_file = SyncSshDirTestConfig::SRC_TEST_DIR + "/ssh_copy/sshd_config";
    EXPECT_TRUE(fs::exists(expected_file)) << "sshd_config wurde nicht kopiert";

    // Regression: SyncFile::copy_source_to_dest() called fs::last_write_time() on
    // the SSH source path unconditionally, which threw and returned before
    // ++copy_count_ for every single file - the copy itself still succeeded
    // (the assertions above passed even while broken), so only copyCount
    // catches it. This is also what the GUI's progress bar is driven by, so a
    // stuck copyCount showed up there as a progress bar that never moved.
    // Not pinned to an exact count: DEST_TEST_DIR is shared with
    // SyncLocalDirToRemote above and only cleaned once per process, so how
    // many files end up here depends on test order.
    EXPECT_GT(copy_count, 0u) << "copyCount wurde bei SSH-Quelle nicht hochgezaehlt (Fortschrittsanzeige haengt)";

    // Cleanup: Lokales Testverzeichnis entfernen; DEST_TEST_DIR gehoert dem
    // SSH-Testkonto (per SSH nach dorthin gesynct) und muss deshalb per SFTP
    // geloescht werden, ein lokales fs::remove_all() haette keinen Zugriff.
    {
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
        tu::erase_ssh_test_dir(L"localhost:" + std::wstring(
            SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()));
    }
}

// Test 3: Leeres lokales Verzeichnis nach Remote synchronisieren
TEST_F(SyncSshDirTest, SyncEmptyLocalDirToRemote)
{
    namespace fs = std::filesystem;
    
    // Leeres Verzeichnis erstellen
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR + "/empty_dir";
    fs::create_directories(local_dir);
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_empty";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    
    // Sollte keine Exception werfen
    EXPECT_NO_THROW(test());
}

// Test 4: Verzeichnis mit vielen Dateien synchronisieren
TEST_F(SyncSshDirTest, SyncDirWithManyFiles)
{
    namespace fs = std::filesystem;
    
    // Viele Dateien erstellen
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR + "/many_files";
    fs::create_directories(local_dir);
    
    const int file_count = 50;
    for (int i = 0; i < file_count; ++i) {
        std::ofstream(local_dir + "/file_" + std::to_string(i) + ".txt") 
            << "Content of file " << i;
    }
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_many";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    
    // Stichprobenartig einige Dateien prüfen
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo info0 = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_many/file_0.txt");
    RemoteFileInfo info25 = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_many/file_25.txt");
    RemoteFileInfo info49 = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_many/file_49.txt");
    
    EXPECT_TRUE(info0.exists);
    EXPECT_TRUE(info25.exists);
    EXPECT_TRUE(info49.exists);
}

// Test 5: Verzeichnis mit Dateien synchronisieren (nur Dateien im Root-Verzeichnis)
// Hinweis: SyncDir kopiert keine Unterverzeichnisse, nur Dateien im angegebenen Verzeichnis
TEST_F(SyncSshDirTest, SyncDirectoryWithFiles)
{
    namespace fs = std::filesystem;
    
    // Dateien im Hauptverzeichnis erstellen
    fs::create_directories(SyncSshDirTestConfig::SRC_TEST_DIR);
    fs::create_directories(SyncSshDirTestConfig::DEST_TEST_DIR);

    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "root.txt")) << "root";
    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "data.txt")) << "data";
    std::ofstream(tu::concat_path(SyncSshDirTestConfig::SRC_TEST_DIR, "config.txt")) << "config";
    
    std::wstring src_path = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(), SyncSshDirTestConfig::SRC_TEST_DIR.end()) + PATH_SEPARATOR_W;
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    EXPECT_EQ(copy_count, 3);
    // Prüfen der Dateien
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo root = inspector.inspect_file(
        tu::concat_path(SyncSshDirTestConfig::DEST_TEST_DIR, "root.txt"));
    RemoteFileInfo data = inspector.inspect_file(
        tu::concat_path(SyncSshDirTestConfig::DEST_TEST_DIR, "data.txt"));
    RemoteFileInfo config = inspector.inspect_file(
        tu::concat_path(SyncSshDirTestConfig::DEST_TEST_DIR, "config.txt"));
    
    EXPECT_TRUE(root.exists);
    EXPECT_TRUE(data.exists);
    EXPECT_TRUE(config.exists);

    // Cleanup: Lokales Testverzeichnis entfernen; DEST_TEST_DIR gehoert dem
    // SSH-Testkonto (per SSH nach dorthin gesynct) und muss deshalb per SFTP
    // geloescht werden, ein lokales fs::remove_all() haette keinen Zugriff.
    {
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
        tu::erase_ssh_test_dir(L"localhost:" + std::wstring(
            SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()));
    }
}

// Test 6: Verzeichnis mit verschiedenen Dateitypen
TEST_F(SyncSshDirTest, SyncDirWithDifferentFileTypes)
{
    namespace fs = std::filesystem;
    
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR + "/mixed_types";
    fs::create_directories(local_dir);
    
    // Verschiedene Dateitypen erstellen
    std::ofstream(local_dir + "/text.txt") << "Text file";
    std::ofstream(local_dir + "/script.sh") << "#!/bin/bash\necho 'test'";
    std::ofstream(local_dir + "/data.json") << "{\"key\": \"value\"}";
    std::ofstream(local_dir + "/config.xml") << "<?xml version=\"1.0\"?><root/>";
    
    // Binäre Datei
    std::ofstream bin_file(local_dir + "/binary.bin", std::ios::binary);
    std::vector<char> data(1024, 'B');
    bin_file.write(data.data(), data.size());
    bin_file.close();
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_mixed";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test(si, copy_count, false);
    test();
    
    // Prüfen aller Dateitypen
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo txt = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_mixed/text.txt");
    RemoteFileInfo sh = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_mixed/script.sh");
    RemoteFileInfo bin = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_mixed/binary.bin");
    
    EXPECT_TRUE(txt.exists);
    EXPECT_TRUE(sh.exists);
    EXPECT_TRUE(bin.exists);
    EXPECT_EQ(bin.size_bytes, 1024);
}

// Test 7: Existierendes Remote-Verzeichnis überschreiben
TEST_F(SyncSshDirTest, SyncToExistingRemoteDir)
{
    namespace fs = std::filesystem;
    
    // Erste Synchronisation
    std::string local_dir = SyncSshDirTestConfig::SRC_TEST_DIR + "/overwrite_test";
    fs::create_directories(local_dir);
    std::ofstream(local_dir + "/initial.txt") << "initial content";
    
    std::wstring src_path = std::wstring(local_dir.begin(), local_dir.end()) + L"/";
    std::wstring dest_path = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_overwrite";
    
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(src_path);
    si.target(dest_path);
    
    std::size_t copy_count = 0;
    SyncDir test1(si, copy_count, false);
    test1();
    
    // Zweite Synchronisation mit neuer Datei
    std::ofstream(local_dir + "/updated.txt") << "updated content";

    // Synchronize erneut starten für zweiten Lauf
    Synchronize::start();

    std::size_t copy_count2 = 0;
    SyncDir test2(si, copy_count2, false);
    test2();
    
    // Beide Dateien sollten auf Remote existieren
    SshFileInspector inspector(SyncSshDirTestConfig::create_test_account());
    RemoteFileInfo initial = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_overwrite/initial.txt");
    RemoteFileInfo updated = inspector.inspect_file(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_overwrite/updated.txt");
    
    EXPECT_TRUE(initial.exists);
    EXPECT_TRUE(updated.exists);
}
