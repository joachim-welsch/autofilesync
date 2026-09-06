#pragma once
#include "SyncBase.h"
#ifdef _TEST
#include <atomic>
#endif

namespace AFS_Core {

class SyncFile: public SyncBase {
    bool transferFilename_ = false;

    void copySourceToDest(const std::wstring& source, const std::wstring& destination, uint64_t size);
    void copyDestToSource(const std::wstring& destination, const std::wstring& source, uint64_t size);
    bool saveCopyFile(const std::wstring& src, const std::wstring& dest);
    CopyResult coreCopyFunction(const std::wstring& src, const std::wstring& dest);
    void preCopy(const std::wstring& src, const std::wstring& dest);
    void setSsh();
    bool sshSource() const;
    bool sshDest() const;
public:
    SyncFile(const SyncItem& si, std::size_t& copyCount, bool onlyCount = false, bool transferFilename = false);
    void operator()();
#ifdef _TEST
    bool testSaveCopyFile(const std::wstring& src, const std::wstring& dest) { return saveCopyFile(src, dest); }
    void testCopyDestToSource(const std::wstring& destination, const std::wstring& source, uint64_t size = 0) { copyDestToSource(destination, source, size); }
#endif
};

#ifdef _TEST
extern std::atomic<int> waitForUpdatedFiletimeCallCount;
#endif

inline
bool SyncFile::sshDest() const { return ssh_ == SSHPATH::dest || ssh_ == SSHPATH::both; }

inline
bool SyncFile::sshSource() const { return ssh_ == SSHPATH::source || ssh_ == SSHPATH::both; }

} // namespace AFS_Core
