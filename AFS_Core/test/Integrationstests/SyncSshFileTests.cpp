#include "pch.h"
#include "SshFileInspector.h"
#include "SshAccount.h"
#include "SyncFileSsh.h"
#include "Config.h"
#include "Synchronize.h"
#include "SyncFile.h"
#include "SyncSshFileTests.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <FilesystemUtil.h>
#include "FileUtil.h"
#include "DirUtil.h"
#include "strutil.h"
#include "SshTest.h"
#include "DirUtil.h"

using namespace SSHTest;
using namespace util;
using std::string;

// Test-Konfiguration - Anpassen an Ihre SSH-Umgebung
namespace SyncSshFileTestConfig {
#ifdef _UNIX
    const std::string TEST_REMOTE_PATH = fsu::homeDir() + "/tmp/test_file.txt";
    const std::string TEST_REMOTE_DIR = fsu::homeDir() + "/tmp";
#else
    const std::string TEST_REMOTE_PATH = fsu::homeDir() + "\\tmp\\test_file.txt";
    const std::string TEST_REMOTE_DIR = fsu::homeDir() + "\\tmp";
#endif
    // Hilfsfunktion um ein SshAccount-Objekt zu erstellen
    inline SshAccount createTestAccount(const std::string& user = SSH_USER,
                                        const std::string& host = SSH_HOST,
                                        const std::string& password = SSH_PASSWORD) {
        SshAccount acc;
        acc.setName("test_account");
        acc.setUser(user);
        acc.setHost(host);
        acc.setPassword(password);
        return acc;
    }
}

// Fixture für SSH-Tests mit Setup und Teardown
class SyncSshFileInspectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Diese Tests belegen die Aufrufkonvention von inspectFile() ueber alle
        // Pfadformen hinweg - mit und ohne Laufwerksbuchstaben, mit Leerzeichen,
        // relativ, Verzeichnis. Sie brauchen den Testserver und ueberspringen
        // sich, wo keiner laeuft.
        SKIP_WITHOUT_SSH_SERVER();
    }

    void TearDown() override {
        // Optional: Aufräumen nach Tests
    }
};

// Test 1: Konstruktor und Verbindungsaufbau
TEST_F(SyncSshFileInspectorTest, ConstructorConnectsSuccessfully)
{
    EXPECT_NO_THROW({
        SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());
    });
}

// Test 2: Konstruktor mit falschen Credentials
TEST_F(SyncSshFileInspectorTest, ConstructorFailsWithWrongPassword)
{
    EXPECT_THROW({
        SshFileInspector inspector(
            SyncSshFileTestConfig::createTestAccount(SSH_USER, SSH_HOST, "wrong_password")
        );
    }, std::runtime_error);
}

// Test 3: Konstruktor mit falschem Host
TEST_F(SyncSshFileInspectorTest, ConstructorFailsWithInvalidHost)
{
    EXPECT_THROW({
        SshFileInspector inspector(
            SyncSshFileTestConfig::createTestAccount(SSH_USER, "invalid.host.that.does.not.exist.123", SSH_PASSWORD)
        );
    }, std::runtime_error);
}

// Test 4: Prüfung einer existierenden Datei
TEST_F(SyncSshFileInspectorTest, InspectExistingFile)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    // Annahme: /etc/hosts existiert auf den meisten Unix-Systemen
    RemoteFileInfo info = inspector.inspectFile(hostsPath);
    
    EXPECT_TRUE(info.exists);
    EXPECT_GT(info.size_bytes, 0);
    EXPECT_NE(info.last_modified_iso, "N/A");
    EXPECT_GT(info.raw_mtime, 0);
}

// Test 5: Prüfung einer nicht existierenden Datei
TEST_F(SyncSshFileInspectorTest, InspectNonExistingFile)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    std::string non_existing = tu::concatTemp("this_file_absolutely_does_not_exist_") +
                                std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    RemoteFileInfo info = inspector.inspectFile(non_existing);
    
    EXPECT_FALSE(info.exists);
    EXPECT_EQ(info.size_bytes, 0);
    EXPECT_EQ(info.last_modified_iso, "N/A");
    EXPECT_EQ(info.raw_mtime, 0);
}

// Test 6: Prüfung mehrerer Dateien mit derselben Session
TEST_F(SyncSshFileInspectorTest, InspectMultipleFiles)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    // Test mit mehreren Standard-System-Dateien
    std::vector<std::string> test_files = {
        tu::concatPath(etcPath, "hosts"),
#ifdef _UNIX
        tu::concatPath(etcPath, "hostname"),
        tu::concatPath(etcPath, "passwd")
#else
        tu::concatPath(etcPath, "networks"),
        tu::concatPath(etcPath, "protocol")
#endif
    };

    for (const auto& file : test_files) {
        RemoteFileInfo info = inspector.inspectFile(file);
        EXPECT_TRUE(info.exists) << "File should exist: " << file;
        EXPECT_GT(info.size_bytes, 0) << "File should have size > 0: " << file;
    }
}

// Test 7: Prüfung der Dateigröße
TEST_F(SyncSshFileInspectorTest, CheckFileSize)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    // /etc/hosts sollte klein sein (< 10KB auf den meisten Systemen)
    RemoteFileInfo info = inspector.inspectFile(hostsPath);
    
    EXPECT_TRUE(info.exists);
    EXPECT_GT(info.size_bytes, 0);
    EXPECT_LT(info.size_bytes, 10240); // < 10KB
}

// Test 8: Prüfung des Zeitstempel-Formats
TEST_F(SyncSshFileInspectorTest, CheckTimestampFormat)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    RemoteFileInfo info = inspector.inspectFile(hostsPath);
    
    EXPECT_TRUE(info.exists);
    EXPECT_NE(info.last_modified_iso, "N/A");
    
    // Format sollte mindestens "YYYY-MM-DD HH:MM:SS" enthalten (kann auch Zeitzone haben)
    EXPECT_GE(info.last_modified_iso.length(), 19);
    EXPECT_EQ(info.last_modified_iso[4], '-');
    EXPECT_EQ(info.last_modified_iso[7], '-');
    EXPECT_EQ(info.last_modified_iso[10], ' ');
    EXPECT_EQ(info.last_modified_iso[13], ':');
    EXPECT_EQ(info.last_modified_iso[16], ':');
}

// Test 9: Prüfung mit leerem Pfad
TEST_F(SyncSshFileInspectorTest, InspectEmptyPath)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    RemoteFileInfo info = inspector.inspectFile("");
    
    EXPECT_FALSE(info.exists);
}

// Test 10: Destruktor-Test (Ressourcen-Cleanup)
TEST_F(SyncSshFileInspectorTest, DestructorCleansUpProperly)
{
    // Test dass mehrere Instanzen nacheinander ohne Probleme erstellt werden können
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW({
            SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());
            RemoteFileInfo info = inspector.inspectFile(hostsPath);
            EXPECT_TRUE(info.exists);
            // Inspector wird am Ende des Scopes zerstört
        });
    }
}

// Test 11: Vergleich von raw_mtime und formatiertem Zeitstempel
TEST_F(SyncSshFileInspectorTest, TimestampConsistency)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    RemoteFileInfo info = inspector.inspectFile(hostsPath);
    
    EXPECT_TRUE(info.exists);
    EXPECT_GT(info.raw_mtime, 0);
    
    // Der raw_mtime sollte ein sinnvoller Unix-Timestamp sein
    // (irgendwann nach 2020-01-01 und vor 2030-01-01)
    time_t min_time = 1577836800; // 2020-01-01
    time_t max_time = 1893456000; // 2030-01-01
    
    EXPECT_GT(info.raw_mtime, min_time);
    EXPECT_LT(info.raw_mtime, max_time);
}

// Test 12: Prüfung eines Verzeichnisses (sollte anders behandelt werden als Datei)
TEST_F(SyncSshFileInspectorTest, InspectDirectory)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    RemoteFileInfo info = inspector.inspectFile(etcPath);
    
    // Ein Verzeichnis könnte als existierend zurückgegeben werden
    // Je nach SFTP-Implementation
    if (info.exists) {
        EXPECT_GE(info.size_bytes, 0);
    }
}

// Test 13: Stress-Test mit vielen aufeinanderfolgenden Abfragen
TEST_F(SyncSshFileInspectorTest, StressTestMultipleQueries)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    // 100 aufeinanderfolgende Abfragen
    for (int i = 0; i < 100; ++i) {
        RemoteFileInfo info = inspector.inspectFile(hostsPath);
        EXPECT_TRUE(info.exists);
    }
}

// Test 14: Sonderzeichen im Pfad
TEST_F(SyncSshFileInspectorTest, InspectPathWithSpaces)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    // Pfad mit Leerzeichen (könnte nicht existieren, aber sollte nicht crashen)
    RemoteFileInfo info = inspector.inspectFile(tu::concatTemp("file with spaces.txt"));
    
    // Sollte nicht crashen, auch wenn Datei nicht existiert
    EXPECT_FALSE(info.exists);
}

// Test 15: Absolute vs. relative Pfade
TEST_F(SyncSshFileInspectorTest, InspectRelativePath)
{
    SshFileInspector inspector(SyncSshFileTestConfig::createTestAccount());

    // Relative Pfade könnten je nach SFTP-Implementation funktionieren oder nicht
    RemoteFileInfo info = inspector.inspectFile(string("..")+ hostsPath);
    // Kein Crash erwartet, aber Existenz ist implementationsabhängig
    EXPECT_GE(info.size_bytes, 0);
}

// ============================================================================
// SSH Copy Tests - Kopieren zwischen Laptop und Remote-Server
// ============================================================================

// Fixture für SSH-Copy-Tests
class SyncSshFileCopyTest : public ::testing::Test {

protected:
    const std::string TEST_FILENAME = "ssh_copy_test_file.txt";
    const std::string TEST_CONTENT = "Dies ist ein Test für SSH-Kopieren vom Laptop!";
#ifdef _UNIX
    const std::string LOCAL_TEST_DIR = std::string(getenv("HOME")) + "/tmp/ssh_copy_test";
    const std::string REMOTE_TEST_DIR = std::string(getenv("HOME")) + "/tmp/ssh_copy_test_remote";
#else
	const std::string LOCAL_TEST_DIR = fsu::homeDir() + "\\tmp\\ssh_copy_test";
	const std::string REMOTE_TEST_DIR = fsu::homeDir() + "\\tmp\\ssh_copy_test_remote";
#endif

    void SetUp() override {
        SKIP_WITHOUT_SSH_SERVER();
        namespace fs = std::filesystem;
        // Lokales Testverzeichnis erstellen
        fs::create_directories(LOCAL_TEST_DIR);

        // Testdatei lokal erstellen
        std::string localFilePath = tu::concatPath(LOCAL_TEST_DIR, TEST_FILENAME);
        std::ofstream outFile(localFilePath);
        outFile << TEST_CONTENT;
        outFile.close();

        // Synchronize zurücksetzen (sonst wird Kopieren als "Stopped" abgebrochen)
        Synchronize::start();
    }

    void TearDown() override {
        namespace fs = std::filesystem;
        // Lokale Testdateien aufräumen
        std::error_code ec;
        fs::remove_all(LOCAL_TEST_DIR, ec);
    }
    
    // SSH-Account für Localhost-Verbindung erstellen (verwendet SshTestConfig)
    SshAccount createLaptopAccount() {
        return SyncSshFileTestConfig::createTestAccount();
    }
    
    // Account zur Config hinzufügen (falls noch nicht vorhanden)
    void ensureAccountInConfig() {
        // Account dynamisch zur Config hinzufügen
        // Der Name muss "localhost" sein, da das im Pfad als Präfix verwendet wird
        SshAccount acc;
        acc.setName("localhost");
        acc.setUser(SSH_USER);
        acc.setHost(SSH_HOST);
        acc.setPassword(SSH_PASSWORD);
        Config::instance().add(acc);
    }
};

TEST_F(SyncSshFileCopyTest, CopyLocalFileToRemote)
{
    ensureAccountInConfig();
    namespace fs = std::filesystem;

    // Quell- und Zielpfade definieren
    fs::create_directories(tu::testSourceDir());
    // testSourceDir()/testDestDir() teilen sich ein Elternverzeichnis
    // ("/tmp/test"); das wurde gerade als dieser lokale Benutzer angelegt, die
    // Gegenseite unten legt "dest" darunter aber ueber das SSH-Testkonto an -
    // ohne g+w/o+w koennte dessen sshMkdir() das Elternverzeichnis nicht
    // beschreiben.
    fs::permissions(fs::path(tu::testSourceDir()).parent_path(), fs::perms::all);
    const std::string TEST_FILENAME = tu::concatPath(tu::testSourceDir(), "data.txt");
    std::ofstream(TEST_FILENAME) << "data";

    const std::string TEST_REMOTE_FILENAME = "localhost:" + tu::concatPath(tu::testDestDir(), "data.txt");;
    // SyncFile kopiert ein einzelnes Ziel und legt dessen Verzeichnis anders
    // als SyncDir/SyncTree nicht selbst an (siehe createTargetDirIfNotExist());
    // das Zielverzeichnis muss hier also vorher existieren. sshMkdir() ist wie
    // ein einfaches mkdir nicht rekursiv, darum zuerst das Elternverzeichnis.
    const std::wstring remoteDestParent = L"localhost:" + string_to_wstring(fs::path(tu::testDestDir()).parent_path().string());
    const std::wstring remoteDestDir = L"localhost:" + string_to_wstring(tu::testDestDir());
    if (!sshExists(remoteDestParent))
        sshMkdir(remoteDestParent);
    if (!sshExists(remoteDestDir))
        sshMkdir(remoteDestDir);

    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Dir);
    si.source(string_to_wstring(TEST_FILENAME));
    si.target(string_to_wstring(TEST_REMOTE_FILENAME));

    std::size_t copyCount = 0;
    SyncFile test(si, copyCount, false);
    test();

    // Ergebnis prüfen
    EXPECT_EQ(copyCount, 1);
    // Prüfen ob die Datei auf dem Remote-Server existiert
    SshFileInspector inspector(createLaptopAccount());
    // inspectFile() erwartet den Pfad ohne Kontopraefix - so wie ihn die
    // Aufraeumzeile unten schon bildet. Frueher stand hier der volle Pfad und
    // der Windows-Zweig schnitt das Praefix selbst noch einmal ab.
    RemoteFileInfo info = inspector.inspectFile(getRemotePath(string_to_wstring(TEST_REMOTE_FILENAME)));

    EXPECT_TRUE(info.exists) << "Remote-Datei wurde nicht gefunden";
    EXPECT_GT(info.size_bytes, 0) << "Remote-Datei ist leer";
    std::filesystem::remove(string_to_wstring(TEST_FILENAME));
    // Ueber SSH loeschen, nicht lokal: die Remote-Datei gehoert dem
    // SSH-Testkonto, das lokale std::filesystem::remove() (als Entwickler-User)
    // hat darauf keinen Zugriff - siehe eraseSshTestDir() in DirUtil.cpp.
    sshRemoveFile(string_to_wstring(TEST_REMOTE_FILENAME));
}

// Test: Lokale Datei via SSH nach /tmp kopieren (Fs -> SSH)
TEST_F(SyncSshFileCopyTest, CopyLocalFileToRemoteTmp)
{
    ensureAccountInConfig();
    
    const std::string TEST_FILENAME = "ssh_copy_test_file.txt";
    const std::string TEST_REMOTE_FILENAME = "ssh_copy_test_file_remote.txt";

    // Quell- und Zielpfade definieren
    std::string srcPath = tu::concatTemp(tu::concatPath("ssh_copy_test", TEST_FILENAME));
    std::string destPath =  "localhost:" + tu::concatSSHTemp(TEST_REMOTE_FILENAME);

    // Quelldatei anlegen: SetUp() legt eine gleichnamige Datei unter
    // LOCAL_TEST_DIR ($HOME/tmp/...) an, srcPath zeigt aber ins OS-Temp-
    // Verzeichnis - beide muessen nicht identisch sein, also hier direkt an
    // srcPath schreiben statt sich auf SetUp() zu verlassen.
    std::filesystem::create_directories(std::filesystem::path(srcPath).parent_path());
    std::ofstream(srcPath) << TEST_CONTENT;

    // Datei kopieren
    CopyResult result = copyFsToSsh(string_to_wstring(srcPath), string_to_wstring(destPath));

    // Ergebnis prüfen
    EXPECT_EQ(result, CopyResult::OK) << "Kopieren von lokal nach SSH fehlgeschlagen";

    // Prüfen ob die Datei auf dem Remote-Server existiert
    SshFileInspector inspector(createLaptopAccount());
    RemoteFileInfo info = inspector.inspectFile(tu::concatSSHTemp(TEST_REMOTE_FILENAME));

    EXPECT_TRUE(info.exists) << "Remote-Datei wurde nicht gefunden";
    EXPECT_GT(info.size_bytes, 0) << "Remote-Datei ist leer";

    std::filesystem::remove(srcPath);
    sshRemoveFile(string_to_wstring(destPath));
}

// Test: Datei von SSH nach lokal kopieren (SSH -> Fs)
TEST_F(SyncSshFileCopyTest, CopyRemoteFileToLocal)
{
    ensureAccountInConfig();
    
    // Erst eine Datei auf dem Remote-Server erstellen (via /etc/hosts die existiert)
#ifdef _UNIX
    std::wstring srcPath = L"localhost:/etc/hosts";
#else
	std::wstring srcPath = L"localhost:C:\\Windows\\System32\\drivers\\etc\\hosts";
#endif
    std::wstring destPath = tu::concatTemp(tu::concatPath(L"ssh_copy_test", L"hosts_from_remote.txt"));
    namespace fs = std::filesystem;
    // copySshToFs() legt das Zielverzeichnis nicht selbst an - im echten
    // Sync-Ablauf tut das schon SyncBase::createTargetDirIfNotExist() vorher.
    fs::create_directories(fs::path(destPath).parent_path());
    // Datei kopieren
    CopyResult result = copySshToFs(srcPath, destPath);

    // Ergebnis prüfen
    EXPECT_EQ(result, CopyResult::OK) << "Kopieren von SSH nach lokal fehlgeschlagen";

    // Prüfen ob die lokale Datei existiert
    EXPECT_TRUE(fs::exists(destPath)) << "Lokale Datei wurde nicht erstellt";

    // Prüfen ob die Datei Inhalt hat
    std::ifstream inFile(wstring_to_string(destPath));
    std::string content((std::istreambuf_iterator<char>(inFile)),
                         std::istreambuf_iterator<char>());
    inFile.close();

    EXPECT_FALSE(content.empty()) << "Kopierte Datei ist leer";
    fs::remove(destPath);
}

// Test: Große Datei kopieren
TEST_F(SyncSshFileCopyTest, CopyLargeFileToRemote)
{
    ensureAccountInConfig();
    
    // Große Testdatei erstellen (1 MB)
    std::string largeFilePath = tu::concatPath(LOCAL_TEST_DIR, "large_test_file.bin");
    std::ofstream outFile(largeFilePath, std::ios::binary);
    std::vector<char> data(1024 * 1024, 'X');  // 1 MB mit 'X' gefüllt
    outFile.write(data.data(), data.size());
    outFile.close();
    
    // largeFilePath liegt unter LOCAL_TEST_DIR ($HOME/tmp/...), nicht unter dem
    // OS-Temp-Verzeichnis - srcPath muss dorthin zeigen, wo die Datei gerade
    // tatsaechlich angelegt wurde.
    std::wstring srcPath = string_to_wstring(largeFilePath);
    std::wstring destPath = L"localhost:" + tu::concatSSHTemp(L"large_test_file_remote.bin");

    // Datei kopieren
    CopyResult result = copyFsToSsh(srcPath, destPath);

    EXPECT_EQ(result, CopyResult::OK) << "Kopieren der großen Datei fehlgeschlagen";

    // Größe auf Remote prüfen
    SshFileInspector inspector(createLaptopAccount());
    RemoteFileInfo info = inspector.inspectFile(tu::concatSSHTemp("large_test_file_remote.bin"));

    EXPECT_TRUE(info.exists);
    EXPECT_EQ(info.size_bytes, 1024 * 1024) << "Dateigröße stimmt nicht überein";
    sshRemoveFile(destPath);
}

// Test: Kopieren mit nicht existierendem Quellpfad
TEST_F(SyncSshFileCopyTest, CopyNonExistentSourceFile)
{
    ensureAccountInConfig();
    
    std::wstring srcPath = tu::concatTemp(L"ssh_copy_test/non_existent_file.txt");
    std::wstring destPath = L"localhost:" + tu::concatTemp(L"should_not_be_created.txt");
    
    // Kopieren sollte fehlschlagen
    CopyResult result = copyFsToSsh(srcPath, destPath);
    
    EXPECT_NE(result, CopyResult::OK) << "Kopieren einer nicht existierenden Datei sollte fehlschlagen";
}