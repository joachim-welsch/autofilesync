#pragma once

#include "SyncBase.h"
#include "SyncFileSsh.h"
#include <filesystem>

class SyncDir: public SyncBase {
private:
    void syncSshDirEntry(const SshDirEntry& entry, const std::wstring& sourcePath, const std::wstring& wildcard, const std::wstring& destination);
public:
    SyncDir(const SyncItem& si, std::size_t& copyCount, bool onlyCount);
    void operator()() override;
};
