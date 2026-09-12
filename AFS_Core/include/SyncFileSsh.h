#pragma once

#include <string>
#include <optional>
#include <vector>
#include <atomic>
#include "SshAccount.h"
#include "SyncBase.h"

namespace AFS_Core {

struct SshDirEntry {
    std::wstring name;
    bool is_directory = false;
    bool is_regular_file = false;
    bool is_symlink = false;
    uint64_t size = 0;
};

std::optional<SshAccount> get_account_from_path(const std::wstring& path);
std::string get_remote_path(const std::wstring& path);
std::string win_to_sftp_path(const std::string& win);

CopyResult copy_ssh_to_fs(const std::wstring& src, const std::wstring& dest);
CopyResult copy_ssh_to_ssh(const std::wstring& src, const std::wstring& dest);
CopyResult copy_fs_to_ssh(const std::wstring& src, const std::wstring& dest);

bool ssh_exists(const std::wstring& path);
bool ssh_is_directory(const std::wstring& path);
bool ssh_mkdir(const std::wstring& path);
bool ssh_remove_file(const std::wstring& path);
std::vector<SshDirEntry> ssh_list_dir(const std::wstring& path);

#ifdef _TEST
extern std::atomic<int> ssh_is_directory_call_count;  // [SyncFileSsh.md#ssh_is_directory_call_count]
#endif

} // namespace AFS_Core
