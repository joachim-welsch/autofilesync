#pragma once
#include <syncitem.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class SyncFile
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SyncFile {
    const SyncItem& si_;
    std::size_t&    copyCount_;
    const bool      transferFilename_;

    void copySourceToDest(const std::string& source, const std::string& destination);
    void copyDestToSource(const std::string& destination, const std::string& source);
public:
    SyncFile(const SyncItem& si, std::size_t& copyCount, bool transferFilename=false);
    void operator()();
};
