#pragma once
#include "SyncBase.h"
#include "SyncFileSsh.h"

namespace AFS_Core {

class SyncTree: SyncBase {
protected:
    void sync_branch(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_ssh_branch(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_ssh_dir_entry(const SshDirEntry& entry, const std::wstring& source_path, const std::wstring& wildcard, const std::wstring& destination);
    void sync_local_to_ssh_branch(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void sync_ssh_to_ssh_branch(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
public:
    SyncTree(const SyncItem& si, std::size_t& copy_count, bool only_count);
    void operator()(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void count(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void operator()() override;
};

} // namespace AFS_Core
