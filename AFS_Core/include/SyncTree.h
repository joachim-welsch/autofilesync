#pragma once
#include "SyncBase.h"
#include <set>
#include <vector>

namespace AFS_Core {

class SyncTree: SyncBase {
    // Common view of a local or SSH directory entry
    struct Entry {
        std::wstring name;
        bool is_directory;
        bool is_regular_file;
        bool is_symlink;
    };
    wchar_t source_separator() const;
    wchar_t target_separator() const;
    std::vector<Entry> list_source(const std::wstring& sourcename) const;
    void sync_tree(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    std::set<std::wstring> sync_entries(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_entry(const Entry& entry, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_subtree(const std::wstring& name, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_file_entry(const std::wstring& name, const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
public:
    SyncTree(const SyncItem& si, std::size_t& copy_count, bool only_count);
    void operator()() override;
};

} // namespace AFS_Core
