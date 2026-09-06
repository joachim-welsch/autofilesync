#include "pch.h"
#include "BackupProof.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "Logging.h"
#include "util.h"

namespace AFS_Core {

namespace pt = boost::property_tree;
namespace fs = std::filesystem;
using std::wstring;

namespace {
    // The proofs of one item are collected under one key, so the file stays a
    // single JSON document that can be appended to instead of a concatenation
    // of documents that no parser would accept.
    const wstring entriesKey = L"Sicherungsnachweise";

    thread_local SyncReport* currentReport = nullptr;

    wstring localTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm parts{};
#ifdef WIN32
        localtime_s(&parts, &time);
#else
        parts = *std::localtime(&time);
#endif
        std::wostringstream os;
        os << std::put_time(&parts, L"%Y-%m-%d %H:%M:%S");
        return os.str();
    }

    // A sync item name is free text and lands in a file name here. Everything a
    // path could stumble over becomes '_' rather than being dropped, so two
    // names cannot collapse onto the same file by losing their only difference.
    wstring asFileName(const wstring& itemName)
    {
        const wstring forbidden = L"<>:\"/\\|?*";
        wstring retval;
        for (wchar_t c : itemName)
            retval += (c < 32 || forbidden.find(c) != wstring::npos) ? L'_' : c;
        // Trailing dots and blanks are not preserved by Windows file names.
        while (!retval.empty() && (retval.back() == L'.' || retval.back() == L' '))
            retval.pop_back();
        return retval.empty() ? wstring(L"Sicherungsnachweis") : retval;
    }

    pt::wptree buildEntry(const SyncItem& item, std::size_t files, const SyncReport& report)
    {
        pt::wptree entry;
        entry.put(wstring(L"Zeitstempel"), localTimestamp());
        // Repeated in every entry although the file name already carries it:
        // asFileName() may have changed the name, and an entry that is copied
        // out of the file has to stay attributable on its own.
        entry.put(wstring(L"Name"), item.name());
        entry.put(wstring(L"Quelle"), item.source());
        entry.put(wstring(L"Ziel"), item.target());
        entry.put(wstring(L"Dateien"), std::to_wstring(files));
        entry.put(wstring(L"Datenvolumen"), std::to_wstring(report.bytesTransferd()));
        entry.put(wstring(L"Status"), wstring(report.successful() ? L"Erfolgreich" : L"Fehlgeschlagen"));
        if (!report.errors().empty()) {
            pt::wptree errors;
            for (const wstring& message : report.errors()) {
                pt::wptree node;
                node.put(wstring(L""), message);
                errors.push_back(std::make_pair(wstring(L""), node));
            }
            entry.push_back(std::make_pair(wstring(L"Fehler"), errors));
        }
        return entry;
    }

    // An existing proof file that cannot be parsed is left untouched: it is
    // evidence, and overwriting it with a fresh document would destroy whatever
    // it still holds. read is false in that case, so the caller does not write.
    pt::wptree readProofFile(const fs::path& file, bool& read)
    {
        pt::wptree root;
        read = true;
        std::error_code ec;
        if (!fs::exists(file, ec))
            return root;
        try {
            std::wifstream in(file);
            pt::read_json(in, root);
        }
        catch (const std::exception& ex) {
            LOG_ERROR3(L"Sicherungsnachweis nicht lesbar, Eintrag verworfen: ", file.wstring(),
                util::string_to_wstring(ex.what()))
            read = false;
        }
        return root;
    }

    bool writeProofFile(const fs::path& file, const pt::wptree& root)
    {
        // Written beside the target and renamed over it, like Config::write():
        // a proof file half-written by a crash or a full disk would take the
        // previous entries down with it.
        const fs::path tmpFile = file.wstring() + L".tmp";
        try {
            std::wofstream os(tmpFile, std::ios_base::out | std::ios_base::trunc);
            os.exceptions(std::ios::failbit | std::ios::badbit);
            pt::write_json(os, root);
        }
        catch (const std::exception& ex) {
            LOG_ERROR3(L"Sicherungsnachweis konnte nicht geschrieben werden: ", file.wstring(),
                util::string_to_wstring(ex.what()))
            util::remove_file(tmpFile.wstring());
            return false;
        }
        return util::rename_file(tmpFile.wstring(), file.wstring());
    }
}

void SyncReport::error(const wstring& message)
{
    if (errors_.size() > maxErrors)
        return;
    if (errors_.size() == maxErrors) {
        errors_.push_back(L"Weitere Fehler wurden nicht aufgezeichnet.");
        return;
    }
    errors_.push_back(message);
}

SyncReportGuard::SyncReportGuard(SyncReport* report)
    : previous_(currentReport)
{
    currentReport = report;
}

SyncReportGuard::~SyncReportGuard()
{
    currentReport = previous_;
}

void reportSyncError(const wstring& message)
{
    if (currentReport != nullptr)
        currentReport->error(message);
}

void reportSyncVolume(uint64_t bytes)
{
    if (currentReport != nullptr)
        currentReport->transferred(bytes);
}

fs::path backupProofFilePath(const fs::path& directory, const wstring& itemName)
{
    return directory / (asFileName(itemName) + L".json");
}

bool writeBackupProof(const fs::path& directory, const SyncItem& item, std::size_t files,
    const SyncReport& report)
{
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec) {
        LOG_ERROR3(L"Verzeichnis fuer Sicherungsnachweise nicht anlegbar: ", directory.wstring(),
            util::string_to_wstring(ec.message()))
        return false;
    }
    const fs::path file = backupProofFilePath(directory, item.name());
    bool readable = false;
    pt::wptree root = readProofFile(file, readable);
    if (!readable)
        return false;
    pt::wptree& entries = root.count(entriesKey) ? root.get_child(entriesKey)
                                                : root.put_child(entriesKey, pt::wptree());
    entries.push_back(std::make_pair(wstring(L""), buildEntry(item, files, report)));
    return writeProofFile(file, root);
}

} // namespace AFS_Core
