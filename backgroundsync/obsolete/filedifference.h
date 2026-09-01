#pragma once
#include <string>
#include <syncitem.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class FileDifference
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FileDifference {
public:
    enum Result { undefined, SourceModified, DestModified, DestNotExists, SourceNotExits } result_ = undefined;
private:
#ifdef _WIN32
    const std::wstring  source_;
    const std::wstring  dest_;
#else
    const std::string  source_;
    const std::string  dest_;
#endif
    CompareItems    ci_;
public:
#ifdef _WIN32
    FileDifference(const std::wstring& source, const std::wstring& dest, const CompareItems& ci);
#else
    FileDifference(const std::string& source, const std::string& dest, const CompareItems& ci);
#endif
    void operator()();
    Result result() const { return result_; }
};
