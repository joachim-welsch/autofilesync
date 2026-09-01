#include "stdafx.h"
#include <filedifference.h>
#ifdef USEQT
#include <qapplication.h>
#endif
#include <winutil.h>
#include <util.h>
#include <filesystem>
#include <synchronize.h>
#include "logging.h"

namespace fs=std::filesystem;

#ifdef _WIN32
using std::wstring;
#else
using std::string;
#endif

using namespace util;

#ifdef _WIN32
FileDifference::FileDifference(const wstring& source, const wstring& dest, const CompareItems& ci)
    : source_(source)
    , dest_(dest)
    , ci_(ci)
{}


void FileDifference::operator()()
{
    SYSTEMTIME sourcetime, desttime;
    DWORD sourcesize, destsize;

    if (!util::fileinfo(source_, sourcetime, sourcesize))
        result_=SourceNotExits;
    else if (!util::fileinfo(dest_, desttime, destsize))
        result_=DestNotExists;
    else if (ci_.find(CompareItem::time) != ci_.cend() && desttime > sourcetime)
        result_=DestModified;
    else if (ci_.find(CompareItem::time) != ci_.cend() && sourcetime > desttime)
        result_=SourceModified;
    else if (ci_.find(CompareItem::size) != ci_.cend() && sourcesize > destsize)
        result_=SourceModified;
    else if (ci_.find(CompareItem::size) != ci_.cend() && destsize > sourcesize)
        result_=DestModified;
    if (result_ == undefined && ci_.find(CompareItem::content) != ci_.cend()) {
        //if (sourcesize > 1000000)
        //    Synchronize::setProgressMessage("Vergleiche\n" + util::wstring_to_string(source_)
        //                                    + "\nmit\n" + util::wstring_to_string(dest_) + "...");
        //QCoreApplication::processEvents();
        if (!filesEqual(source_, dest_))
            result_=SourceModified;
    }
	TRACE("FileDifference result: ", result_, " source: ", toString(source_), " dest: ", toString(dest_));
}
#else
FileDifference::FileDifference(const string& source, const string& dest, const CompareItems& ci)
    : source_(source)
    , dest_(dest)
    , ci_(ci)
{}

bool operator >(const fs::file_time_type& a, const fs::file_time_type& b)
{
    return a.time_since_epoch() > b.time_since_epoch();
}

void FileDifference::operator()()
{
    constexpr uint64_t MB = 0x100000;
    fs::file_time_type sourcetime, desttime;
    uint64_t sourcesize, destsize, comparebuffersize = 4096;

    if (!util::fileinfo(source_, sourcesize, sourcetime))
        result_=SourceNotExits;
    else if (!util::fileinfo(dest_, destsize, desttime))
        result_=DestNotExists;
    else if (ci_.find(CompareItem::time) != ci_.cend() && sourcetime > desttime)
        result_=SourceModified;
    else if (ci_.find(CompareItem::size) != ci_.cend() && sourcesize > destsize)
        result_=SourceModified;
    else if (ci_.find(CompareItem::size) != ci_.cend() && destsize > sourcesize)
        result_=DestModified;
    if (result_ == undefined && ci_.find(CompareItem::content) != ci_.cend()) {
        if (sourcesize > MB) {
            Synchronize::setProgressMessage("Vergleiche\n" + source_
                                            + "\nmit\n" + dest_ + "...");
            comparebuffersize=MB;
        }
    }
    if (result_ == undefined && !util::filesEqual(source_, dest_, comparebuffersize))
        result_=SourceModified;
    // TODO remove
    //result_=SourceModified;

}

#endif

