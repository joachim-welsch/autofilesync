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
    const std::string SRC_TEST_DIR = tu::concatPath(tu::testTempDir(), "test_ssh_dir_src");
	const std::string DEST_TEST_DIR = tu::concatPath(tu::testTempDir(), "test_ssh_dir_dest");
    
    // Hilfsfunktion um ein SshAccount-Objekt zu erstellen
    inline SshAccount createTestAccount(const std::string& user = SSH_USER,
                                        const std::string& host = SSH_HOST,
                                        const std::string& password = SSH_PASSWORD) {
        SshAccount acc;
        acc.setName("test_dir_account");
        acc.setUser(user);
        acc.setHost(host);
        acc.setPassword(password);
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
        acc.setName("localhost");
        acc.setUser(SSH_USER);
        acc.setHost(SSH_HOST);
        acc.setPassword(SSH_PASSWORD);
        Config::instance().add(acc);

        // Remote-Zielverzeichnisse aus frueheren Laeufen entfernen: sie
        // gehoeren dem SSH-Testkonto, das lokale fs::remove_all() oben kann
        // sie nicht anfassen - ein stehen gebliebenes Ziel wuerde als
        // "bereits synchronisiert" durchgehen und copyCount faelschlich
        // bei 0 belassen. Jeder Aufruf verbindet neu per SSH, deshalb nur
        // einmal pro Prozess statt vor jedem einzelnen Test.
        static bool sshDestCleaned = false;
        if (!sshDestCleaned) {
            tu::eraseSshTestDirVariants(
                L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()),
                { L"_empty", L"_many", L"_mixed", L"_overwrite" });
            sshDestCleaned = true;
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
    std::string localDir = SyncSshDirTestConfig::SRC_TEST_DIR;
    std::ofstream(tu::concatPath(localDir, "file1.txt")) << "content1";
    std::ofstream(tu::concatPath(localDir, "file2.txt")) << "content2";
    std::ofstream(tu::concatPath(localDir, "file3.txt")) << "content3";
    
    // SyncItem konfigurieren
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + L"/";
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();
    
    // Prüfen ob Dateien auf Remote existieren
    SshFileInspector inspector(SyncSshDirTestConfig::createTestAccount());
    RemoteFileInfo info1 = inspector.inspectFile(SyncSshDirTestConfig::DEST_TEST_DIR + "/file1.txt");
    RemoteFileInfo info2 = inspector.inspectFile(SyncSshDirTestConfig::DEST_TEST_DIR + "/file2.txt");
    RemoteFileInfo info3 = inspector.inspectFile(SyncSshDirTestConfig::DEST_TEST_DIR + "/file3.txt");
    
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

    std::ofstream(tu::concatPath(SyncSshDirTestConfig::SRC_TEST_DIR, "sshd_config")) << "test";
    std::ofstream(tu::concatPath(SyncSshDirTestConfig::SRC_TEST_DIR, "other.txt")) << "other";

    {
        std::wstring seedSrcPath = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(),
            SyncSshDirTestConfig::SRC_TEST_DIR.end()) + L"/";
        std::wstring seedDestPath = L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(),
            SyncSshDirTestConfig::DEST_TEST_DIR.end());

        SyncItem seed;
        seed.syncCycle(0);
        seed.syncItemType(SyncItemType::Dir);
        seed.source(seedSrcPath);
        seed.target(seedDestPath);

        std::size_t seedCopyCount = 0;
        SyncDir seedSync(seed, seedCopyCount, false);
        seedSync();
    }

    // Jetzt Remote -> Local synchronisieren
    std::wstring srcPath = L"localhost:" + std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(),
        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    std::wstring destPath = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(),
        SyncSshDirTestConfig::SRC_TEST_DIR.end()) + L"/ssh_copy";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();

    // Prüfen ob lokales Verzeichnis erstellt wurde
    EXPECT_TRUE(fs::exists(SyncSshDirTestConfig::SRC_TEST_DIR + "/ssh_copy"));

    // Prüfen ob Dateien kopiert wurden (z.B. sshd_config sollte existieren)
    std::string expectedFile = SyncSshDirTestConfig::SRC_TEST_DIR + "/ssh_copy/sshd_config";
    EXPECT_TRUE(fs::exists(expectedFile)) << "sshd_config wurde nicht kopiert";

    // Regression: SyncFile::copySourceToDest() called fs::last_write_time() on
    // the SSH source path unconditionally, which threw and returned before
    // ++copyCount_ for every single file - the copy itself still succeeded
    // (the assertions above passed even while broken), so only copyCount
    // catches it. This is also what the GUI's progress bar is driven by, so a
    // stuck copyCount showed up there as a progress bar that never moved.
    // Not pinned to an exact count: DEST_TEST_DIR is shared with
    // SyncLocalDirToRemote above and only cleaned once per process, so how
    // many files end up here depends on test order.
    EXPECT_GT(copyCount, 0u) << "copyCount wurde bei SSH-Quelle nicht hochgezaehlt (Fortschrittsanzeige haengt)";

    // Cleanup: Lokales Testverzeichnis entfernen; DEST_TEST_DIR gehoert dem
    // SSH-Testkonto (per SSH nach dorthin gesynct) und muss deshalb per SFTP
    // geloescht werden, ein lokales fs::remove_all() haette keinen Zugriff.
    {
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
        tu::eraseSshTestDir(L"localhost:" + std::wstring(
            SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()));
    }
}

// Test 3: Leeres lokales Verzeichnis nach Remote synchronisieren
TEST_F(SyncSshDirTest, SyncEmptyLocalDirToRemote)
{
    namespace fs = std::filesystem;
    
    // Leeres Verzeichnis erstellen
    std::string localDir = SyncSshDirTestConfig::SRC_TEST_DIR + "/empty_dir";
    fs::create_directories(localDir);
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + L"/";
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_empty";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    
    // Sollte keine Exception werfen
    EXPECT_NO_THROW(test());
}

// Test 4: Verzeichnis mit vielen Dateien synchronisieren
TEST_F(SyncSshDirTest, SyncDirWithManyFiles)
{
    namespace fs = std::filesystem;
    
    // Viele Dateien erstellen
    std::string localDir = SyncSshDirTestConfig::SRC_TEST_DIR + "/many_files";
    fs::create_directories(localDir);
    
    const int fileCount = 50;
    for (int i = 0; i < fileCount; ++i) {
        std::ofstream(localDir + "/file_" + std::to_string(i) + ".txt") 
            << "Content of file " << i;
    }
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + L"/";
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_many";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();
    
    // Stichprobenartig einige Dateien prüfen
    SshFileInspector inspector(SyncSshDirTestConfig::createTestAccount());
    RemoteFileInfo info0 = inspector.inspectFile(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_many/file_0.txt");
    RemoteFileInfo info25 = inspector.inspectFile(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_many/file_25.txt");
    RemoteFileInfo info49 = inspector.inspectFile(
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

    std::ofstream(tu::concatPath(SyncSshDirTestConfig::SRC_TEST_DIR, "root.txt")) << "root";
    std::ofstream(tu::concatPath(SyncSshDirTestConfig::SRC_TEST_DIR, "data.txt")) << "data";
    std::ofstream(tu::concatPath(SyncSshDirTestConfig::SRC_TEST_DIR, "config.txt")) << "config";
    
    std::wstring srcPath = std::wstring(SyncSshDirTestConfig::SRC_TEST_DIR.begin(), SyncSshDirTestConfig::SRC_TEST_DIR.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end());
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();
    EXPECT_EQ(copyCount, 3);
    // Prüfen der Dateien
    SshFileInspector inspector(SyncSshDirTestConfig::createTestAccount());
    RemoteFileInfo root = inspector.inspectFile(
        tu::concatPath(SyncSshDirTestConfig::DEST_TEST_DIR, "root.txt"));
    RemoteFileInfo data = inspector.inspectFile(
        tu::concatPath(SyncSshDirTestConfig::DEST_TEST_DIR, "data.txt"));
    RemoteFileInfo config = inspector.inspectFile(
        tu::concatPath(SyncSshDirTestConfig::DEST_TEST_DIR, "config.txt"));
    
    EXPECT_TRUE(root.exists);
    EXPECT_TRUE(data.exists);
    EXPECT_TRUE(config.exists);

    // Cleanup: Lokales Testverzeichnis entfernen; DEST_TEST_DIR gehoert dem
    // SSH-Testkonto (per SSH nach dorthin gesynct) und muss deshalb per SFTP
    // geloescht werden, ein lokales fs::remove_all() haette keinen Zugriff.
    {
        std::error_code ec;
        fs::remove_all(SyncSshDirTestConfig::SRC_TEST_DIR, ec);
        tu::eraseSshTestDir(L"localhost:" + std::wstring(
            SyncSshDirTestConfig::DEST_TEST_DIR.begin(), SyncSshDirTestConfig::DEST_TEST_DIR.end()));
    }
}

// Test 6: Verzeichnis mit verschiedenen Dateitypen
TEST_F(SyncSshDirTest, SyncDirWithDifferentFileTypes)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshDirTestConfig::SRC_TEST_DIR + "/mixed_types";
    fs::create_directories(localDir);
    
    // Verschiedene Dateitypen erstellen
    std::ofstream(localDir + "/text.txt") << "Text file";
    std::ofstream(localDir + "/script.sh") << "#!/bin/bash\necho 'test'";
    std::ofstream(localDir + "/data.json") << "{\"key\": \"value\"}";
    std::ofstream(localDir + "/config.xml") << "<?xml version=\"1.0\"?><root/>";
    
    // Binäre Datei
    std::ofstream binFile(localDir + "/binary.bin", std::ios::binary);
    std::vector<char> data(1024, 'B');
    binFile.write(data.data(), data.size());
    binFile.close();
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + L"/";
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_mixed";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncDir test(si, copyCount, false);
    test();
    
    // Prüfen aller Dateitypen
    SshFileInspector inspector(SyncSshDirTestConfig::createTestAccount());
    RemoteFileInfo txt = inspector.inspectFile(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_mixed/text.txt");
    RemoteFileInfo sh = inspector.inspectFile(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_mixed/script.sh");
    RemoteFileInfo bin = inspector.inspectFile(
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
    std::string localDir = SyncSshDirTestConfig::SRC_TEST_DIR + "/overwrite_test";
    fs::create_directories(localDir);
    std::ofstream(localDir + "/initial.txt") << "initial content";
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + L"/";
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshDirTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshDirTestConfig::DEST_TEST_DIR.end()) + L"_overwrite";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncDir test1(si, copyCount, false);
    test1();
    
    // Zweite Synchronisation mit neuer Datei
    std::ofstream(localDir + "/updated.txt") << "updated content";

    // Synchronize erneut starten für zweiten Lauf
    Synchronize::start();

    std::size_t copyCount2 = 0;
    SyncDir test2(si, copyCount2, false);
    test2();
    
    // Beide Dateien sollten auf Remote existieren
    SshFileInspector inspector(SyncSshDirTestConfig::createTestAccount());
    RemoteFileInfo initial = inspector.inspectFile(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_overwrite/initial.txt");
    RemoteFileInfo updated = inspector.inspectFile(
        SyncSshDirTestConfig::DEST_TEST_DIR + "_overwrite/updated.txt");
    
    EXPECT_TRUE(initial.exists);
    EXPECT_TRUE(updated.exists);
}
