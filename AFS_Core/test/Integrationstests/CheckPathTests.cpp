#include "pch.h"
#include "CheckPath.h"
#include "config.h"
#include "SshAccount.h"
#include "SshTest.h"
#include "FileUtil.h"
#include "DirUtil.h"
#include <filesystem>
#include <thread>
#include <fstream>
#include <util.h>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace {

    // Windows gibt Verzeichnisse erst frei, wenn das letzte Handle zu ist.
    // Kurz wiederholen statt einmal blind zu versuchen.
    bool removeAllRetry(const fs::path& p, int attempts = 20)
    {
        for (int i = 0; i < attempts; ++i) {
            std::error_code ec;
            fs::remove_all(p, ec);
            if (!fs::exists(p))
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        return !fs::exists(p);
    }

    struct TempTree {
        fs::path path;
        explicit TempTree(fs::path p) : path(std::move(p)) { removeAllRetry(path); }
        ~TempTree() { removeAllRetry(path); }
    };

} // namespace

// Fehler 10: the cache's backoff interval used to double on every lookup that
// hit an entry, even one still within its validity window - not just when the
// entry actually expired and got re-verified. That let the interval balloon
// far past its intended base within a fraction of a second under repeated
// polling (e.g. Synchronize's continuous-sync loop), making the cache appear
// valid long after it should have expired and been re-checked.
TEST(CheckPathTests, RepeatedHitsWithinValidityWindowDoNotExtendIt)
{
    using namespace std::chrono;
    constexpr auto kValidity = milliseconds(2000);
    TempTree tree(fs::temp_directory_path() /
        ("autofilesync_checkpath_test_" + std::to_string(util::currentPid())));

    const fs::path& base = tree.path;
    //const fs::path base = fs::temp_directory_path() / ("autofilesync_checkpath_test_" + std::to_string(getpid()));
    fs::remove_all(base);

    ASSERT_TRUE(fs::create_directories(base / "source"));
    ASSERT_TRUE(fs::create_directories(base / "target"));

    const std::wstring source = (base / "source").wstring();
    const std::wstring target = (base / "target").wstring();

    CheckPath checkPath;
    const auto t0 = steady_clock::now();
    EXPECT_TRUE(checkPath(source, target, false));

    // Remove the source now: further calls only see this if they actually
    // re-check the filesystem instead of returning the cached (stale) result.
    // Quelle entfernen: nachfolgende Aufrufe sehen das nur, wenn sie das
    // Dateisystem wirklich neu pruefen statt den Cache zu liefern.
    ASSERT_TRUE(removeAllRetry(base / "source"))
        << "Quellverzeichnis liess sich nicht loeschen - Vorbedingung verletzt";
    
    // Several hits well within the base 2s interval - none of these may grow
    // the interval past its original 2s, or the cache would stay "valid" (and
    // thus keep returning the stale true) past when it actually should expire.
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(checkPath(source, target, false)) << "Durchlauf " << i;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_LT(steady_clock::now() - t0, kValidity)
        << "Schleife hat das Gueltigkeitsfenster ueberschritten - "
        "Timing auf dieser Maschine zu unzuverlaessig fuer die Aussage";

    // Absolute Deadline statt fixem Restdelta: unempfindlich gegen
   // aufsummierte sleep-Ungenauigkeiten.
    std::this_thread::sleep_until(t0 + kValidity + milliseconds(300));
    // CheckPath::pathExists() now checks the source path itself on Windows
    // too, not just the drive letter (see CheckPath.cpp), so this matches
    // the Unix expectation on both platforms.
    EXPECT_FALSE(checkPath(source, target, false));

    fs::remove_all(base);
}

// Ein nicht erreichbarer Pfad wurde mit demselben Backoff behandelt wie ein
// erreichbarer: das Intervall verdoppelte sich bis 4096s. Nach einem Boot-Race
// (Daemon startet vor dem CIFS-Mount) oder einem Netzwechsel blieb der Sync so
// ueber eine Stunde blind, obwohl das Ziel laengst wieder da war.
TEST(CheckPathTests, UnreachableBackoffIsCappedFarBelowTheReachableOne)
{
    uint16_t interval = 2;
    for (int i = 0; i < 20; ++i)
        interval = nextCheckInterval(interval, true, false);
    EXPECT_EQ(interval, 300);
}

TEST(CheckPathTests, ReachableBackoffStopsAtItsLimitInsteadOfOvershooting)
{
    uint16_t interval = 2;
    for (int i = 0; i < 20; ++i)
        interval = nextCheckInterval(interval, true, true);
    EXPECT_EQ(interval, 3600);
}

TEST(CheckPathTests, BackoffGrowsByDoublingUntilTheCap)
{
    EXPECT_EQ(nextCheckInterval(2, true, false), 4);
    EXPECT_EQ(nextCheckInterval(128, true, false), 256);
    // Verdopplung wuerde ueber den Deckel schiessen -> auf den Deckel begrenzt.
    EXPECT_EQ(nextCheckInterval(256, true, false), 300);
    EXPECT_EQ(nextCheckInterval(2048, true, true), 3600);
}

// Wechselt der Zustand (Ziel taucht auf oder verschwindet), muss wieder
// engmaschig geprueft werden statt am alten Backoff festzuhalten.
TEST(CheckPathTests, ChangedResultResetsBackoffToTheBaseInterval)
{
    EXPECT_EQ(nextCheckInterval(300, false, true), 2);
    EXPECT_EQ(nextCheckInterval(3600, false, false), 2);
}

// Fehler 2 (round 4): withoutVolumeSpec() stripped whenever the path merely
// contained "(" and ")" - not only for a real volume spec ("Label (C:)rest").
// A plain path with parentheses ("Docs (2023)") degenerated to a wrong or
// empty path, so the item never synced; worse, the mangled source looked like
// SourceNotExits, which deletes target files when deleteInTarget is enabled.
TEST(CheckPathTests, WithoutVolumeSpecKeepsPathsThatMerelyContainParentheses)
{
    EXPECT_EQ(withoutVolumeSpec(L"/home/user/Docs (2023)/file.txt"), L"/home/user/Docs (2023)/file.txt");
    EXPECT_EQ(withoutVolumeSpec(L"/media/stick (dev-sdb1)"), L"/media/stick (dev-sdb1)");
    EXPECT_EQ(withoutVolumeSpec(L"/plain/path/file.txt"), L"/plain/path/file.txt");
}

TEST(CheckPathTests, WithoutVolumeSpecStillStripsRealVolumeSpec)
{
    EXPECT_EQ(withoutVolumeSpec(L"Volume (C:)\\dir\\file.txt"), L"C:\\dir\\file.txt");
}

namespace {

// Registriert ein unerreichbares und - wo vorhanden - das lokale Testkonto.
// ".invalid" laesst sich laut RFC 2606 nie aufloesen, der Verbindungsversuch
// scheitert also schnell und ueberall gleich.
class CheckPathSshTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Config::instance().reset();
        SshAccount unreachable;
        unreachable.setName("unreachable");
        unreachable.setUser("nobody");
        unreachable.setHost("this-host-does-not-exist.invalid");
        unreachable.setPassword("irrelevant");
        Config::instance().add(unreachable);

        SshAccount local;
        local.setName("localhost");
        local.setUser(SSHTest::SSH_USER);
        local.setHost(SSHTest::SSH_HOST);
        local.setPassword(SSHTest::SSH_PASSWORD);
        Config::instance().add(local);
    }

    void TearDown() override { Config::instance().reset(); }
};

} // namespace

// Der Kern der Sache: solange die Gegenstelle nicht antwortet, ist der Pfad
// nicht erreichbar - vorher lieferte CheckPath fuer jeden SSH-Pfad ungeprueft
// true, und ein nicht erreichbarer Server sah aus wie ein bereitstehender.
TEST_F(CheckPathSshTest, UnreachableHostIsNotReachable)
{
    CheckPath checkPath;
    EXPECT_FALSE(checkPath.sshPathReachable(L"unreachable:/tmp", true));
    EXPECT_FALSE(checkPath.sshPathReachable(L"unreachable:/tmp", false))
        << "auch ohne Existenzforderung muss die Verbindung selbst zustande kommen";
}

// Ein Praefix ohne passendes Konto ist kein SSH-Pfad, den man erreichen
// koennte - er laesst sich nicht einmal aufloesen.
TEST_F(CheckPathSshTest, UnknownAccountIsNotReachable)
{
    CheckPath checkPath;
    EXPECT_FALSE(checkPath.sshPathReachable(L"gibtesnicht:/tmp", true));
}

// Ein unerreichbarer SSH-Pfad muss auch durch operator()() hindurch dazu
// fuehren, dass die Synchronisation aussetzt - dort wird das Ergebnis
// tatsaechlich ausgewertet.
TEST_F(CheckPathSshTest, OperatorReportsUnreachableSshSource)
{
    CheckPath checkPath;
    const std::wstring target = tu::tempDirW();
    EXPECT_FALSE(checkPath(L"unreachable:/remote/quelle", target, true));
}

TEST_F(CheckPathSshTest, ExistingRemotePathIsReachable)
{
    SKIP_WITHOUT_SSH_SERVER();
    // Verzeichnis anlegen statt sich auf eines zu verlassen: der Test soll an
    // der Erreichbarkeit scheitern koennen, nicht an einer Annahme ueber die
    // Maschine. Schreibweise wie in den uebrigen SSH-Tests, ohne abschliessenden
    // Trenner.
    const std::string dir = tu::concatPath(tu::testTempDir(), "afx_checkpath_ssh");
    fs::create_directories(dir);
    const std::wstring remote = L"localhost:" + util::string_to_wstring(dir);

    CheckPath checkPath;
    EXPECT_TRUE(checkPath.sshPathReachable(remote, true)) << util::wstring_to_string(remote);

    // Mit abschliessendem Trenner eingetragene Pfade sind ueblich und muessen
    // dasselbe Ergebnis liefern - sonst wuerde die Synchronisation still
    // aussetzen, obwohl die Gegenstelle bereitsteht.
    EXPECT_TRUE(checkPath.sshPathReachable(remote + L"\\", true)) << "mit abschliessendem Backslash";
    EXPECT_TRUE(checkPath.sshPathReachable(remote + L"/", true)) << "mit abschliessendem Schraegstrich";

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// Verbunden, aber der Pfad ist weg: fuer eine Quelle ist das ein Grund
// auszusetzen. Sonst raeumte ein geloeschtes Quellverzeichnis zusammen mit
// "Geloeschte Dateien im Ziel ebenfalls loeschen" das Ziel leer.
TEST_F(CheckPathSshTest, MissingRemoteSourcePathIsNotReachable)
{
    SKIP_WITHOUT_SSH_SERVER();
    CheckPath checkPath;
    const std::wstring missing = L"localhost:" + tu::tempDirW() + L"/gibt_es_sicher_nicht_4711";
    EXPECT_FALSE(checkPath.sshPathReachable(missing, true));
}

// Fuer ein Ziel genuegt die Verbindung: ein noch nicht existierendes
// Zielverzeichnis legen SyncDir und SyncTree selbst an.
TEST_F(CheckPathSshTest, MissingRemoteTargetPathIsStillReachable)
{
    SKIP_WITHOUT_SSH_SERVER();
    CheckPath checkPath;
    const std::wstring missing = L"localhost:" + tu::tempDirW() + L"/gibt_es_sicher_nicht_4711";
    EXPECT_TRUE(checkPath.sshPathReachable(missing, false));
}
