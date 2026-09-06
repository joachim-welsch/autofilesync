#pragma once
#include <string>
#include "SyncItem.h"

namespace AFS_Core {

class FileDifference {
public:
    enum class Result { Equal, SourceModified, DestModified, DestNotExists, SourceNotExits,
                        SourceDriveNotMounted, DestDriveNotMounted, Undefined };
private:
    const std::wstring& source_;
    const std::wstring& dest_;
    uint64_t sourcesize_=0, destsize_=0;
    CompareItems    ci_;
    SSHPATH         ssh_ {SSHPATH::none};
    bool            twoWaySync_ = false;
    Result result_ = Result::Equal;

public:
    FileDifference(const std::wstring& source, const std::wstring& dest, const CompareItems& ci, SSHPATH ssh = SSHPATH::none, bool twoWaySync = false);
    uint64_t operator()();
    Result result() const { return result_; }
    uint64_t sourceSize() const { return sourcesize_; }
    uint64_t destSize() const { return destsize_; }
};

} // namespace AFS_Core
