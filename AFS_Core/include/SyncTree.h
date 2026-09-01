#pragma once
#include "SyncBase.h"
#include "SyncFileSsh.h"

class SyncTree: SyncBase {
protected:
    void syncBranch(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void syncSshBranch(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void syncSshDirEntry(const SshDirEntry& entry, const std::wstring& sourcePath, const std::wstring& wildcard, const std::wstring& destination);
    void syncLocalToSshBranch(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void syncSshToSshBranch(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
public:
    SyncTree(const SyncItem& si, std::size_t& copyCount, bool onlyCount);
    void operator()(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void count(const std::wstring& sourcename, const std::wstring& targetname, const std::wstring& wildcard);
    void operator()() override;
};
