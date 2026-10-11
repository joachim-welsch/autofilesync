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
#include "CopyLog.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "Logging.h"
#include "Translation.h"
#include "Util.h"

namespace AFS_Core {

namespace pt = boost::property_tree;
namespace fs = std::filesystem;
using std::wstring;

namespace {

    thread_local SyncReport* current_report = nullptr;

    wstring local_timestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm parts{};
#ifdef WIN32
        localtime_s(&parts, &time);
#else
        localtime_r(&time, &parts);
#endif
        std::wostringstream os;
        os << std::put_time(&parts, L"%Y-%m-%d %H:%M:%S");
        return os.str();
    }

    wstring as_file_name(const wstring& item_name)
    {
        const wstring forbidden = L"<>:\"/\\|?*";
        wstring retval;
        for (wchar_t c : item_name)
            retval += (c < 32 || forbidden.find(c) != wstring::npos) ? L'_' : c;
        while (!retval.empty() && (retval.back() == L'.' || retval.back() == L' '))  // [CopyLog.md#trailing-dots-and-blanks]
            retval.pop_back();
        return retval.empty() ? wstring(L"Sicherungsnachweis") : retval;  // [CopyLog.md#untranslated-file-name]
    }

    void add_field(pt::wptree& entry, const wstring& name, const wstring& value)  // [CopyLog.md#add_field]
    {
        entry.push_back(std::make_pair(name, pt::wptree(value)));
    }

    pt::wptree error_list(const WStringVector& messages)
    {
        pt::wptree errors;
        for (const wstring& message : messages)
            add_field(errors, L"", message);
        return errors;
    }

    pt::wptree build_entry(const SyncItem& item, std::size_t files, const SyncReport& report)  // [CopyLog.md#build_entry]
    {
        using translation::translate;
        pt::wptree entry;
        add_field(entry, translate("backup_proof_field_timestamp"), local_timestamp());
        add_field(entry, translate("backup_proof_field_name"), item.name());  // [CopyLog.md#name-in-every-entry]
        add_field(entry, translate("backup_proof_field_source"), item.source());
        add_field(entry, translate("backup_proof_field_target"), item.target());
        add_field(entry, translate("backup_proof_field_files"), std::to_wstring(files));
        add_field(entry, translate("backup_proof_field_volume"), std::to_wstring(report.bytes_transferd()));
        add_field(entry, translate("backup_proof_field_status"),
            report.successful() ? translate("backup_proof_status_successful") : translate("backup_proof_status_failed"));
        if (!report.errors().empty())
            entry.push_back(std::make_pair(translate("backup_proof_field_errors"), error_list(report.errors())));
        return entry;
    }

    pt::wptree read_copy_log_file(const fs::path& file, bool& read)
    {
        pt::wptree root;
        read = true;
        std::error_code ec;
        if (!fs::exists(file, ec))
            return root;
        try {
            std::ifstream in(file);  // [CopyLog.md#utf8-file]
            std::ostringstream bytes;
            bytes << in.rdbuf();
            std::wistringstream text(util::string_to_wstring(bytes.str()));
            pt::read_json(text, root);
        }
        catch (const std::exception& ex) {
            log_error(translation::translate("backup_proof_not_readable"), file.wstring(),
                util::string_to_wstring(ex.what()));
            read = false;
        }
        return root;
    }

    pt::wptree& entries_of(pt::wptree& root)  // [CopyLog.md#entries_of]
    {
        if (root.empty())
            root.push_back(std::make_pair(translation::translate("backup_proof_entries_key"), pt::wptree()));
        return root.front().second;
    }

    bool write_copy_log_file(const fs::path& file, const pt::wptree& root)
    {
        const fs::path tmp_file = file.wstring() + L".tmp";
        try {
            std::wostringstream text;
            pt::write_json(text, root);
            const std::string bytes = util::wstring_to_string(text.str());  // [CopyLog.md#utf8-file]
            std::ofstream os(tmp_file, std::ios_base::out | std::ios_base::trunc);
            os.exceptions(std::ios::failbit | std::ios::badbit);
            os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }
        catch (const std::exception& ex) {
            log_error(translation::translate("backup_proof_not_writable"), file.wstring(),
                util::string_to_wstring(ex.what()));
            util::remove_file(tmp_file.wstring());
            return false;
        }
        return util::rename_file(tmp_file.wstring(), file.wstring());
    }
}

void SyncReport::error(const wstring& message)
{
    if (errors_.size() > MAX_ERRORS)
        return;
    if (errors_.size() == MAX_ERRORS) {
        errors_.push_back(translation::translate("error_list_capped"));
        return;
    }
    errors_.push_back(message);
}

SyncReportGuard::SyncReportGuard(SyncReport* report)
    : previous_(current_report)
{
    current_report = report;
}

SyncReportGuard::~SyncReportGuard()
{
    current_report = previous_;
}

void report_sync_error(const wstring& message)
{
    if (current_report != nullptr)
        current_report->error(message);
}

void report_sync_volume(uint64_t bytes)
{
    if (current_report != nullptr)
        current_report->transferred(bytes);
}

fs::path copy_log_file_path(const fs::path& directory, const wstring& item_name)
{
    return directory / (as_file_name(item_name) + L".json");
}

bool write_copy_log(const fs::path& directory, const SyncItem& item, std::size_t files,
    const SyncReport& report)
{
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec) {
        log_error(translation::translate("backup_proof_dir_not_creatable"), directory.wstring(),
            util::string_to_wstring(ec.message()));
        return false;
    }
    const fs::path file = copy_log_file_path(directory, item.name());
    bool readable = false;
    pt::wptree root = read_copy_log_file(file, readable);
    if (!readable)
        return false;
    entries_of(root).push_back(std::make_pair(wstring(L""), build_entry(item, files, report)));
    return write_copy_log_file(file, root);
}

} // namespace AFS_Core
