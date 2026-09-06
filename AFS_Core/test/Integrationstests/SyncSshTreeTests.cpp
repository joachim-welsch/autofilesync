#include "pch.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "DirUtil.h"
#include "SyncSshTreeTests.h"
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

// Test-Konfiguration - Anpassen an Ihre SSH-Umgebung
namespace SyncSshTreeTestConfig {
    const std::string SRC_TEST_DIR = tu::concatPath(tu::testTempDir(), "test_ssh_dir_src");
    const std::string DEST_TEST_DIR = tu::concatPath(tu::testTempDir(), "test_ssh_dir_dest");
    
    // Hilfsfunktion um ein SshAccount-Objekt zu erstellen
    inline SshAccount createTestAccount(const std::string& user = SSH_USER,
                                        const std::string& host = SSH_HOST,
                                        const std::string& password = SSH_PASSWORD) {
        SshAccount acc;
        acc.setName("test_tree_account");
        acc.setUser(user);
        acc.setHost(host);
        acc.setPassword(password);
        return acc;
    }
}

// Fixture für SSH-Tree-Tests
class SyncSshTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        namespace fs = std::filesystem;

        SKIP_WITHOUT_SSH_SERVER();

        // Lokales Testverzeichnis erstellen und aufräumen
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
        fs::create_directories(SyncSshTreeTestConfig::SRC_TEST_DIR);
        
        // Account zur Config hinzufügen
        SshAccount acc;
        acc.setName("localhost");
        acc.setUser(SSH_USER);
        acc.setHost(SSH_HOST);
        acc.setPassword(SSH_PASSWORD);
            Config::instance().add(acc);

            // Remote-Zielverzeichnisse aus frueheren Laeufen entfernen: sie
            // gehoeren dem SSH-Testkonto, das lokale fs::remove_all() oben
            // kann sie nicht anfassen - ein stehen gebliebenes Ziel wuerde
            // als "bereits synchronisiert" durchgehen. Jeder Aufruf verbindet
            // neu per SSH, deshalb nur einmal pro Prozess statt vor jedem
            // einzelnen Test.
            static bool sshDestCleaned = false;
            if (!sshDestCleaned) {
                tu::eraseSshTestDirVariants(
                    L"localhost:" + std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), SyncSshTreeTestConfig::DEST_TEST_DIR.end()),
                    { L"_reverse", L"_empty", L"_mixed", L"_many", L"_deep", L"_types", L"_large", L"_project", L"_special" });
                sshDestCleaned = true;
            }

            // Synchronize zurücksetzen
            Synchronize::start();
        }

    void TearDown() override {
        namespace fs = std::filesystem;
        
        // Lokales Testverzeichnis aufräumen
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
};

// Test 1: Lokalen verschachtelten Verzeichnisbaum nach SSH synchronisieren
TEST_F(SyncSshTreeTest, SyncLocalTreeToRemoteWithNestedDirectories)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle verschachtelte Verzeichnisstruktur
    fs::create_directories(tu::concatPath(tu::concatPath(localDir, "subdir1"), "subdir2"));
    fs::create_directories(tu::concatPath(localDir, "subdir3"));
    
    // Erstelle Dateien in verschiedenen Ebenen
    std::ofstream(tu::concatPath(localDir, "root_file.txt")) << "root content";
    std::ofstream(tu::concatPath(tu::concatPath(localDir, "subdir1"), "sub1_file.txt")) << "subdir1 content";
    std::ofstream(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "subdir1"), "subdir2"), "sub2_file.txt")) << "subdir2 content";
    std::ofstream(tu::concatPath(tu::concatPath(localDir, "subdir3"), "sub3_file.txt")) << "subdir3 content";
    
    // SyncItem konfigurieren
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end());
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Prüfen ob Dateien auf Remote existieren
    SshFileInspector inspector(SyncSshTreeTestConfig::createTestAccount());
    
    RemoteFileInfo rootFile = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR, "root_file.txt"));
    RemoteFileInfo sub1File = inspector.inspectFile(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR, "subdir1"), "sub1_file.txt"));
    RemoteFileInfo sub2File = inspector.inspectFile(tu::concatPath(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR, "subdir1"), "subdir2"), "sub2_file.txt"));
    RemoteFileInfo sub3File = inspector.inspectFile(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR, "subdir3"), "sub3_file.txt"));
    
    EXPECT_TRUE(rootFile.exists) << "root_file.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(sub1File.exists) << "sub1_file.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(sub2File.exists) << "sub2_file.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(sub3File.exists) << "sub3_file.txt wurde nicht auf Remote gefunden";

    // Cleanup: Lokales Testverzeichnis entfernen; DEST_TEST_DIR gehoert dem
    // SSH-Testkonto (per SSH nach dorthin gesynct) und muss deshalb per SFTP
    // geloescht werden, ein lokales fs::remove_all() haette keinen Zugriff.
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
        tu::eraseSshTestDir(L"localhost:" + std::wstring(
            SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), SyncSshTreeTestConfig::DEST_TEST_DIR.end()));
    }
}

// Test 2: Remote-Verzeichnisbaum nach lokal synchronisieren
TEST_F(SyncSshTreeTest, SyncRemoteTreeToLocal)
{
    namespace fs = std::filesystem;
    
    // Zuerst: Lokale Testdaten nach Remote kopieren, um sicherzustellen dass Remote-Daten existieren
    std::string localSetupDir = tu::concatPath(SyncSshTreeTestConfig::SRC_TEST_DIR, "setup_for_remote");
    fs::create_directories(tu::concatPath(localSetupDir, "subdir"));
    std::ofstream(tu::concatPath(localSetupDir, "remote_file.txt")) << "remote content";
    std::ofstream(tu::concatPath(tu::concatPath(localSetupDir, "subdir"), "nested_file.txt")) << "nested content";
    
    // Kopiere nach Remote
    std::wstring setupSrcPath = std::wstring(localSetupDir.begin(), localSetupDir.end()) + pathSeparatorW;
    std::wstring remoteDestPath = L"localhost:" + 
                                  std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                              SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_reverse";
    
    SyncItem setupSi;
    setupSi.syncCycle(0);
    setupSi.syncItemType(SyncItemType::Tree);
    setupSi.source(setupSrcPath);
    setupSi.target(remoteDestPath);
    
    std::size_t setupCopyCount = 0;
    SyncTree setupSync(setupSi, setupCopyCount, false);
    setupSync();
    
    // Jetzt: Remote nach lokal synchronisieren
    Synchronize::start();
    std::wstring srcPath = L"localhost:" + 
                           std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(),
                                       SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_reverse";
    std::wstring destPath = std::wstring(SyncSshTreeTestConfig::SRC_TEST_DIR.begin(),
                                         SyncSshTreeTestConfig::SRC_TEST_DIR.end()) + pathSeparatorW + L"remote_copy";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Prüfen ob lokales Verzeichnis erstellt wurde
    EXPECT_TRUE(fs::exists(tu::concatPath(SyncSshTreeTestConfig::SRC_TEST_DIR, "remote_copy")));
    
    // Prüfen ob Dateien vom Remote kopiert wurden
    EXPECT_TRUE(fs::exists(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::SRC_TEST_DIR, "remote_copy"), "remote_file.txt")))
        << "remote_file.txt wurde nicht von Remote kopiert";
    EXPECT_TRUE(fs::exists(tu::concatPath(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::SRC_TEST_DIR, "remote_copy"), "subdir"), "nested_file.txt")))
        << "nested_file.txt wurde nicht von Remote kopiert";

    // Cleanup: Lokales Testverzeichnis entfernen; das per SSH beschriebene
    // Remote-Verzeichnis war DEST_TEST_DIR + "_reverse" (siehe remoteDestPath
    // oben), gehoert dem SSH-Testkonto und muss deshalb per SFTP geloescht
    // werden, ein lokales fs::remove_all() haette weder Zugriff noch den
    // richtigen Pfad getroffen.
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
        tu::eraseSshTestDir(L"localhost:" + std::wstring(
            SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_reverse");
    }
}

// Test 3: Leere Verzeichnisstruktur nach Remote synchronisieren
TEST_F(SyncSshTreeTest, SyncEmptyTreeToRemote)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle nur leere Verzeichnisstruktur
    fs::create_directories(tu::concatPath(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "empty1"), "empty2"), "empty3"), ""));
    fs::create_directories(tu::concatPath(localDir, "empty4"));
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_empty";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    
    // Sollte keine Exception werfen
    EXPECT_NO_THROW(test());

    // Cleanup: Lokale Testverzeichnisse entfernen
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

// Test 4: Gemischter Baum (Dateien und Verzeichnisse) nach Remote synchronisieren
TEST_F(SyncSshTreeTest, SyncMixedTreeToRemote)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle gemischte Struktur
    fs::create_directories(tu::concatPath(localDir, "dir1"));
    fs::create_directories(tu::concatPath(tu::concatPath(localDir, "dir2"), "nested"));
    
    std::ofstream(tu::concatPath(localDir, "file1.txt")) << "content1";
    std::ofstream(tu::concatPath(tu::concatPath(localDir, "dir1"), "file2.txt")) << "content2";
    std::ofstream(tu::concatPath(tu::concatPath(localDir, "dir1"), "file3.md")) << "# Markdown";
    std::ofstream(tu::concatPath(tu::concatPath(localDir, "dir2"), "file4.txt")) << "content4";
    std::ofstream(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "dir2"), "nested"), "file5.txt")) << "content5";
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_mixed";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Prüfe alle Dateien auf Remote
    SshFileInspector inspector(SyncSshTreeTestConfig::createTestAccount());
    
    RemoteFileInfo file1 = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "file1.txt"));
    RemoteFileInfo file2 = inspector.inspectFile(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "dir1"), "file2.txt"));
    RemoteFileInfo file3 = inspector.inspectFile(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "dir1"), "file3.md"));
    RemoteFileInfo file4 = inspector.inspectFile(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "dir2"), "file4.txt"));
    RemoteFileInfo file5 = inspector.inspectFile(tu::concatPath(tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_mixed", "dir2"), "nested"), "file5.txt"));
    
    EXPECT_TRUE(file1.exists) << "file1.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(file2.exists) << "file2.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(file3.exists) << "file3.md wurde nicht auf Remote gefunden";
    EXPECT_TRUE(file4.exists) << "file4.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(file5.exists) << "file5.txt wurde nicht auf Remote gefunden";

    // Cleanup: Lokale Testverzeichnisse entfernen
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

// Test 5: Baum mit vielen Dateien nach Remote synchronisieren
TEST_F(SyncSshTreeTest, SyncTreeWithManyFilesToRemote)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle Verzeichnis mit vielen Dateien
    fs::create_directories(tu::concatPath(localDir, "many_files"));
    
    const int fileCount = 50;
    for (int i = 0; i < fileCount; ++i) {
        std::string filename = tu::concatPath(tu::concatPath(localDir, "many_files"), "file_" + std::to_string(i) + ".txt");
        std::ofstream(filename) << "Content of file " << i;
    }
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_many";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Stichprobenartig einige Dateien prüfen
    SshFileInspector inspector(SyncSshTreeTestConfig::createTestAccount());
    RemoteFileInfo info0 = inspector.inspectFile(
        tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_many", "many_files"), "file_0.txt"));
    RemoteFileInfo info25 = inspector.inspectFile(
        tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_many", "many_files"), "file_25.txt"));
    RemoteFileInfo info49 = inspector.inspectFile(
        tu::concatPath(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_many", "many_files"), "file_49.txt"));
    
    EXPECT_TRUE(info0.exists) << "file_0.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(info25.exists) << "file_25.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(info49.exists) << "file_49.txt wurde nicht auf Remote gefunden";

    // Cleanup: Lokale Testverzeichnisse entfernen
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

// Test 6: Tiefe Verzeichnishierarchie nach Remote synchronisieren
TEST_F(SyncSshTreeTest, SyncDeepHierarchyToRemote)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle sehr tiefe Verzeichnisstruktur
    std::string deepPath = localDir;
    for (int i = 0; i < 8; ++i) {
        deepPath = tu::concatPath(deepPath, "level" + std::to_string(i));
    }
    fs::create_directories(deepPath);
    std::ofstream(tu::concatPath(deepPath, "deep_file.txt")) << "Deep content";
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_deep";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Prüfe, dass die tiefe Struktur kopiert wurde
    SshFileInspector inspector(SyncSshTreeTestConfig::createTestAccount());
    std::string remoteDeepPath = SyncSshTreeTestConfig::DEST_TEST_DIR + "_deep";
    for (int i = 0; i < 8; ++i) {
        remoteDeepPath = tu::concatPath(remoteDeepPath, "level" + std::to_string(i));
    }
    RemoteFileInfo deepFile = inspector.inspectFile(tu::concatPath(remoteDeepPath, "deep_file.txt"));
    
    EXPECT_TRUE(deepFile.exists) << "deep_file.txt wurde nicht auf Remote gefunden";

    // Cleanup: Lokale Testverzeichnisse entfernen
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

// Test 7: Verschiedene Dateitypen nach Remote synchronisieren
TEST_F(SyncSshTreeTest, SyncDifferentFileTypesToRemote)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle Dateien mit verschiedenen Endungen
    std::ofstream(tu::concatPath(localDir, "document.txt")) << "Text file";
    std::ofstream(tu::concatPath(localDir, "script.sh")) << "#!/bin/bash\necho 'test'";
    std::ofstream(tu::concatPath(localDir, "config.json")) << "{\"key\": \"value\"}";
    std::ofstream(tu::concatPath(localDir, "data.xml")) << "<?xml version=\"1.0\"?><root/>";
    std::ofstream(tu::concatPath(localDir, "README.md")) << "# README";
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_types";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Prüfe alle verschiedenen Dateitypen
    SshFileInspector inspector(SyncSshTreeTestConfig::createTestAccount());
    
    RemoteFileInfo txt = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "document.txt"));
    RemoteFileInfo sh = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "script.sh"));
    RemoteFileInfo json = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "config.json"));
    RemoteFileInfo xml = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "data.xml"));
    RemoteFileInfo md = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_types", "README.md"));
    
    EXPECT_TRUE(txt.exists) << "document.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(sh.exists) << "script.sh wurde nicht auf Remote gefunden";
    EXPECT_TRUE(json.exists) << "config.json wurde nicht auf Remote gefunden";
    EXPECT_TRUE(xml.exists) << "data.xml wurde nicht auf Remote gefunden";
    EXPECT_TRUE(md.exists) << "README.md wurde nicht auf Remote gefunden";

    // Cleanup: Lokale Testverzeichnisse entfernen
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

// Test 8: Große Datei nach Remote synchronisieren
TEST_F(SyncSshTreeTest, SyncLargeFileToRemote)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle eine größere Datei (1 MB)
    std::ofstream largeFile(tu::concatPath(localDir, "large_file.dat"), std::ios::binary);
    std::string chunk(1024, 'X'); // 1 KB
    for (int i = 0; i < 1024; ++i) { // 1 MB
        largeFile << chunk;
    }
    largeFile.close();
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_large";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Prüfe, dass die große Datei kopiert wurde
    SshFileInspector inspector(SyncSshTreeTestConfig::createTestAccount());
    RemoteFileInfo largeFileInfo = inspector.inspectFile(
        SyncSshTreeTestConfig::DEST_TEST_DIR + "_large/large_file.dat");
    
    EXPECT_TRUE(largeFileInfo.exists) << "large_file.dat wurde nicht auf Remote gefunden";
    
    // Prüfe Dateigröße (sollte ungefähr 1 MB sein)
    auto sourceSize = fs::file_size(tu::concatPath(localDir, "large_file.dat"));
    EXPECT_EQ(largeFileInfo.size_bytes, sourceSize);

    // Cleanup: Lokale Testverzeichnisse entfernen
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

// Test 9: Komplexe Projektstruktur nach Remote synchronisieren
TEST_F(SyncSshTreeTest, SyncComplexProjectStructureToRemote)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle komplexe Struktur mit mehreren Ebenen
    fs::create_directories(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "project"), "src"), "main"));
    fs::create_directories(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "project"), "src"), "utils"));
    fs::create_directories(tu::concatPath(tu::concatPath(localDir, "project"), "tests"));
    fs::create_directories(tu::concatPath(tu::concatPath(localDir, "project"), "docs"));
    fs::create_directories(tu::concatPath(tu::concatPath(localDir, "project"), "build"));
    
    // Dateien in verschiedenen Verzeichnissen
    std::ofstream(tu::concatPath(tu::concatPath(localDir, "project"), "README.md")) << "# Project";
    std::ofstream(tu::concatPath(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "project"), "src"), "main"), "main.cpp")) << "int main() {}";
    std::ofstream(tu::concatPath(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "project"), "src"), "main"), "config.h")) << "#pragma once";
    std::ofstream(tu::concatPath(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "project"), "src"), "utils"), "helper.cpp")) << "// helper";
    std::ofstream(tu::concatPath(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "project"), "src"), "utils"), "helper.h")) << "// helper.h";
    std::ofstream(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "project"), "tests"), "test_main.cpp")) << "// test";
    std::ofstream(tu::concatPath(tu::concatPath(tu::concatPath(localDir, "project"), "docs"), "manual.txt")) << "Manual";
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_project";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Prüfe komplette Struktur auf Remote
    SshFileInspector inspector(SyncSshTreeTestConfig::createTestAccount());
    
    std::string remoteBase = tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_project", "project");
    
    RemoteFileInfo readme = inspector.inspectFile(tu::concatPath(remoteBase, "README.md"));
    RemoteFileInfo mainCpp = inspector.inspectFile(tu::concatPath(tu::concatPath(tu::concatPath(tu::concatPath(remoteBase, "src"), "main"), "main.cpp"), ""));
    RemoteFileInfo configH = inspector.inspectFile(tu::concatPath(tu::concatPath(tu::concatPath(remoteBase, "src"), "main"), "config.h"));
    RemoteFileInfo helperCpp = inspector.inspectFile(tu::concatPath(tu::concatPath(tu::concatPath(remoteBase, "src"), "utils"), "helper.cpp"));
    RemoteFileInfo helperH = inspector.inspectFile(tu::concatPath(tu::concatPath(tu::concatPath(remoteBase, "src"), "utils"), "helper.h"));
    RemoteFileInfo testMain = inspector.inspectFile(tu::concatPath(tu::concatPath(remoteBase, "tests"), "test_main.cpp"));
    RemoteFileInfo manual = inspector.inspectFile(tu::concatPath(tu::concatPath(remoteBase, "docs"), "manual.txt"));
    
    EXPECT_TRUE(readme.exists) << "README.md wurde nicht auf Remote gefunden";
    EXPECT_TRUE(mainCpp.exists) << "main.cpp wurde nicht auf Remote gefunden";
    EXPECT_TRUE(configH.exists) << "config.h wurde nicht auf Remote gefunden";
    EXPECT_TRUE(helperCpp.exists) << "helper.cpp wurde nicht auf Remote gefunden";
    EXPECT_TRUE(helperH.exists) << "helper.h wurde nicht auf Remote gefunden";
    EXPECT_TRUE(testMain.exists) << "test_main.cpp wurde nicht auf Remote gefunden";
    EXPECT_TRUE(manual.exists) << "manual.txt wurde nicht auf Remote gefunden";

    // Cleanup: Lokale Testverzeichnisse entfernen
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}

// Test 10: Dateien mit speziellen Zeichen im Namen nach Remote synchronisieren
TEST_F(SyncSshTreeTest, SyncFilesWithSpecialCharactersToRemote)
{
    namespace fs = std::filesystem;
    
    std::string localDir = SyncSshTreeTestConfig::SRC_TEST_DIR;
    
    // Erstelle Dateien mit speziellen Zeichen (Unix-kompatibel)
    std::ofstream(tu::concatPath(localDir, "file_with_spaces.txt")) << "spaces";
    std::ofstream(tu::concatPath(localDir, "file-with-dashes.txt")) << "dashes";
    std::ofstream(tu::concatPath(localDir, "file_with_underscores.txt")) << "underscores";
    std::ofstream(tu::concatPath(localDir, "file.multiple.dots.txt")) << "dots";
    
    std::wstring srcPath = std::wstring(localDir.begin(), localDir.end()) + pathSeparatorW;
    std::wstring destPath = L"localhost:" + 
                            std::wstring(SyncSshTreeTestConfig::DEST_TEST_DIR.begin(), 
                                        SyncSshTreeTestConfig::DEST_TEST_DIR.end()) + L"_special";
    
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(srcPath);
    si.target(destPath);
    
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();
    
    // Prüfe alle Dateien mit speziellen Zeichen
    SshFileInspector inspector(SyncSshTreeTestConfig::createTestAccount());
    
    RemoteFileInfo spaces = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_special", "file_with_spaces.txt"));
    RemoteFileInfo dashes = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_special", "file-with-dashes.txt"));
    RemoteFileInfo underscores = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_special", "file_with_underscores.txt"));
    RemoteFileInfo dots = inspector.inspectFile(tu::concatPath(SyncSshTreeTestConfig::DEST_TEST_DIR + "_special", "file.multiple.dots.txt"));
    
    EXPECT_TRUE(spaces.exists) << "file_with_spaces.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(dashes.exists) << "file-with-dashes.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(underscores.exists) << "file_with_underscores.txt wurde nicht auf Remote gefunden";
    EXPECT_TRUE(dots.exists) << "file.multiple.dots.txt wurde nicht auf Remote gefunden";

    // Cleanup: Lokale Testverzeichnisse entfernen
    {
        std::error_code ec;
        fs::remove_all(SyncSshTreeTestConfig::SRC_TEST_DIR, ec);
    }
}
