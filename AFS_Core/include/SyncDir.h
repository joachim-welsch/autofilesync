#pragma once

#include "SyncBase.h"
#include "SyncFileSsh.h"
#include <filesystem>

namespace AFS_Core {

class SyncDir: public SyncBase {
private:
    void sync_ssh_dir_entry(const SshDirEntry& entry, const std::wstring& source_path, const std::wstring& wildcard, const std::wstring& destination);
public:
    SyncDir(const SyncItem& si, std::size_t& copy_count, bool only_count);
    void operator()() override;
};

} // namespace AFS_Core
