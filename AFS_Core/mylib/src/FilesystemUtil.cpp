#include "pch.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "MountedDrives.h"  // Hinzugefügter Include
#include <strutil.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#ifdef WIN32
    #include <Windows.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <unistd.h>
    #include  <cctype>
#endif

using std::string;
using std::wstring;

namespace fs = std::filesystem;

// Helper function to convert file_time_type to time_t
template <typename TP>
static std::time_t to_time_t(TP tp)
{
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
              + system_clock::now());
    return system_clock::to_time_t(sctp);
}

namespace FilesystemUtil {

#ifdef _WIN32
    std::string homeDir()
    {
        char* homeDrive = getenv("HOMEDRIVE");
        char* homePath = getenv("HOMEPATH");
        if (homeDrive && homePath) {
            return std::string(homeDrive) + std::string(homePath);
        }
        return ""; // Fallback, falls die Umgebungsvariablen nicht gesetzt sind
    }
#else
    std::string homeDir()
    {
        const char* homedir;
        if ((homedir = getenv("HOME")) == NULL) {
            homedir = getpwuid(getuid())->pw_dir;
        }
        return homedir;
    }
#endif

bool fileExists(const std::wstring& StrFilename)
{
    return fs::exists(fs::status(StrFilename));
}

wstring makeErrorMessage(FileInfoResult result)
{
    switch (result) {
    case FileInfoResult::FileNotExist:
        return L"Datei existiert nicht";
    case FileInfoResult::DriveNotMounted:
        return L"Laufwerk ist nicht gemounted";
    case FileInfoResult::CannotReadFileAttributes:
        return L"Kann Dateiattribute nicht lesen";
    case FileInfoResult::InvalidFileTime:
        return L"Ungueltige Dateizeit";
    case FileInfoResult::ErrorReadFileInfo:
        return L"Fehler beim Lesen der Dateiinformationen";
    default:
        return L"Unbekannter Fehler";
    }
}

#ifdef WIN32

FileInfoResult fileinfo(const std::wstring& fname, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    if (!fileExists(fname)) {
        // Prüfen, ob das Laufwerk gemounted ist
        fs::path filePath(fname);
        auto& mountedDrives = MountedDrives::instance();
        if (!mountedDrives.driveMounted(filePath)) {
            return FileInfoResult::DriveNotMounted;
        }
        return FileInfoResult::FileNotExist;
    }
    
    _WIN32_FILE_ATTRIBUTE_DATA data{};
    // GetFileAttributesEx returns BOOL (0 on failure), not the DWORD
    // INVALID_FILE_ATTRIBUTES sentinel GetFileAttributes() uses - comparing
    // against 0xFFFFFFFF here never matched, so a real failure (permission
    // denied, deleted mid-call) fell through and used the zeroed-out 'data'.
    BOOL rc = GetFileAttributesEx(fname.c_str(), GetFileExInfoStandard, &data);
    if (!rc) {
        return FileInfoResult::CannotReadFileAttributes;
    }
    SYSTEMTIME stUTC;
    // Kept purely as the validity check it always was; the value below no
    // longer goes through it.
    if (!FileTimeToSystemTime(&data.ftLastWriteTime, &stUTC))
        return FileInfoResult::InvalidFileTime;
    // FILETIME and file_time_type share both epoch (1601-01-01 UTC) and tick
    // (100 ns) on Windows, so the raw counter carries over unchanged. The
    // detour this replaces converted to local time and then to a time_t -
    // seconds since 1970, stored as if they were ticks since 1601, which put
    // every timestamp 369 years into the past, shifted it by the time zone
    // offset and truncated it to whole seconds.
    ULARGE_INTEGER ticks;
    ticks.LowPart = data.ftLastWriteTime.dwLowDateTime;
    ticks.HighPart = data.ftLastWriteTime.dwHighDateTime;
    last_write_time = fs::file_time_type(
        fs::file_time_type::duration(static_cast<int64_t>(ticks.QuadPart)));
    // nFileSizeLow alone silently truncated any file >= 4GB to its low 32
    // bits, e.g. reporting a 4.5GB file as ~0.5GB - FileDifference's size
    // comparison would then think it needs a re-copy on every single pass.
    filesize = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    return FileInfoResult::OK;
}
#else
FileInfoResult fileinfo(const std::wstring& fpathname, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    try {
        if (!fs::exists(fpathname)) {
            // Prüfen, ob das Laufwerk gemounted ist
            auto& mountedDrives = MountedDrives::instance();
            if (!mountedDrives.driveMounted(fpathname)) {
                return FileInfoResult::DriveNotMounted;
            }
            return FileInfoResult::FileNotExist;
        }
        
        filesize = fs::file_size(fpathname);
        last_write_time = fs::last_write_time(fpathname);

#ifdef _DEBUG
        std::time_t tt = to_time_t(last_write_time);
        std::tm* gmt = std::gmtime(&tt);
        std::stringstream buffer;
        buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M");
        std::string formattedFileTime = buffer.str();

        std::wstringstream debug_msg;
        debug_msg << L"Fileinfo result -> Filesize: " << filesize << L" last_write_time: " << util::string_to_wstring(formattedFileTime);
        LOG_DEBUG(debug_msg.str())
#endif
        return FileInfoResult::OK;
    }
    catch (std::exception& ex) {
        return FileInfoResult::ErrorReadFileInfo;
    }
}
#endif

bool touch(const wstring& fpathname)
{
    try {
        auto now = fs::file_time_type::clock::now(); // aktuelle Zeit im Dateisystem-Zeitformat
        fs::last_write_time(fpathname, now);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR2(L"Error in function touch: ", e.what() )
        return false;
    }
}

static fs::path normalize_best_effort(const fs::path& p) {
    std::error_code ec;
    fs::path n = fs::weakly_canonical(p, ec);
    return ec ? fs::path(p) : n;
}

// lstat wrapper (folgt Symlinks nicht)
static bool lstat_ok(const fs::path& p, struct stat& st) {
#ifndef WIN32
    return ::lstat(p.c_str(), &st) == 0;
#else
    // Windows: verwende stat (kein lstat verfügbar)
    return ::stat(p.string().c_str(), &st) == 0;
#endif
}

static std::string decode_mountinfo_escapes(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size() &&
            std::isdigit(static_cast<unsigned char>(s[i+1])) &&
            std::isdigit(static_cast<unsigned char>(s[i+2])) &&
            std::isdigit(static_cast<unsigned char>(s[i+3]))) {
            int val = (s[i+1]-'0')*64 + (s[i+2]-'0')*8 + (s[i+3]-'0');
            out.push_back(static_cast<char>(val));
            i += 3;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static inline void strip_trailing_slash(std::string& s) {
    if (s.size() > 1 && s.back() == '/') s.pop_back();
}

static std::optional<fs::path> nonroot_covering_mountpoint(const fs::path& target) {
#ifdef __linux__
    std::ifstream f("/proc/self/mountinfo");
    if (!f) return std::nullopt;

    // Ziel normalisieren
    std::string t = normalize_best_effort(target).string();
    strip_trailing_slash(t);
    if (t.empty()) t = "/";

    // tiefsten passenden Mount sammeln
    std::optional<fs::path> best;
    size_t best_len = 0;

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string field, mp_enc;

        // Felder 1..4 überspringen, Feld 5 ist Mountpoint (escaped)
        for (int i = 0; i < 4; ++i) {
            if (!(iss >> field)) { mp_enc.clear(); break; }
        }
        if (!(iss >> mp_enc)) continue;

        std::string mp = decode_mountinfo_escapes(mp_enc);
        strip_trailing_slash(mp);
        if (mp.empty()) mp = "/";

        // Root nie zurückgeben
        if (mp == "/") continue;

        // exakter Treffer
        if (t == mp) {
            // Exakt ist immer der tiefstmögliche Kandidat
            return fs::path(mp);
        }

        // Präfix-Match mit Pfadgrenze: mp/...
        if (t.size() > mp.size() &&
            t.compare(0, mp.size(), mp) == 0 &&
            t[mp.size()] == '/')
        {
            if (mp.size() > best_len) {
                best_len = mp.size();
                best = fs::path(mp);
            }
        }
    }

    return best; // std::nullopt, wenn nur "/" oder gar nichts passt
#else
    (void)target; // TODO windows implementieren
    return std::nullopt;
#endif
}

std::optional<fs::path> isMountpoint(const fs::path& path)
{
#ifdef __linux__
    // Auf Linux ist mountinfo die robusteste Quelle (erkennt auch Bind-Mounts).
    return nonroot_covering_mountpoint(path);
#else
    // Fallback: Device-Grenze zwischen Pfad und Parent (erkennt keine Bind-Mounts).
    fs::path p = normalize_best_effort(path);
    struct stat cur{}, par{};
    if (!lstat_ok(p, cur)) return std::nullopt;

    fs::path parent = p.has_parent_path() ? p.parent_path() : fs::path("/");
    if (!lstat_ok(parent, par)) return std::nullopt;

    if (cur.st_dev != par.st_dev)
        return p;
    return std::nullopt;
#endif
}

std::optional<fs::path> getMountRoot(const fs::path& in)
{
#ifdef WIN32
    // Auf Windows: Laufwerksbuchstabe extrahieren (z.B. "Z:\")
    // Wir verwenden 'in' direkt, um den Laufwerksbuchstaben zu erhalten
    // (normalize_best_effort kann UNC-Pfade zurückgeben)
    if (in.has_root_name()) {
        fs::path root = in.root_name();
        root /= "\\"; // Füge Backslash hinzu für "Z:\"
        return root;
    }
    // Handle Unix-style paths (e.g., "/home/...") on Windows
    // These can occur when using WSL paths or cross-platform test data
    std::wstring pathStr = in.wstring();
    if (!pathStr.empty() && pathStr[0] == L'/') {
        return fs::path("/");
    }
    return std::nullopt;
#else
    fs::path current = normalize_best_effort(in);

    // Wenn 'in' nicht existiert, gehe so weit hoch, bis etwas existiert.
    struct stat stCur{};
    while (!lstat_ok(current, stCur)) {
        if (!current.has_parent_path()) return std::nullopt;
        current = current.parent_path();
    }

    for (;;) {
        fs::path parent = current.has_parent_path() ? current.parent_path() : fs::path("/");
        struct stat stPar{};
        if (!lstat_ok(parent, stPar)) return std::nullopt;

        if (stCur.st_dev != stPar.st_dev) {
            // Geräte unterscheiden sich: 'current' ist Mountpoint der darunterliegenden Struktur
            return current.empty() ? fs::path("/") : current;
        }

        if (current == "/") {
            // Wurzel des aktuellen Namespace/FS erreicht -> das ist immer ein Mountpoint
            return fs::path("/");
        }

        // Aufwärts gehen: Parent wird neuer Current
        current = parent;
        stCur = stPar;
    }
#endif
}

} // namespace FilesystemUtil
