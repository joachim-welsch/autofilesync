#include "stdafx.h"
#include <syncfile.h>
#include <util.h>
#include <winutil.h>
#include <filesystem>
#include <iostream>
#include <synchronize.h>
#include <filedifference.h>
#include <sstream>
#include <ctime>
#include "logging.h"
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

using std::cout;
using std::string;
namespace fs=std::filesystem;

#ifdef _WIN32
using std::wstring;
#endif

void transferFilename(const string& source, string& dest)
{
    dest = util::withTrailingChar(dest, '/');
    //dest += util::filename(source); works not for linux
    fs::path p(source);
    dest += p.filename().string();
}


SyncFile::SyncFile(const SyncItem& si, std::size_t& copyCount, bool transferFilename)
    : si_(si)
    , copyCount_(copyCount)
    , transferFilename_(transferFilename)
{}

#ifdef _WIN32
void moveFileToBackup(const wstring& destFilename)
{
    int i=1;
    string newDestFilename;
    do {
        std::ostringstream os;
        os << util::wstring_to_string(destFilename) << ".bak." << i;
        newDestFilename=os.str();
        ++i;
    } while(util::fileExists(newDestFilename));
//    try {
        rename(util::wstring_to_string(destFilename).c_str(), newDestFilename.c_str());
    //}
    //catch(filesystem_error& err) {
    //    std::ostringstream os;
    //    os << "Fehler: " << err.what() << ", beim Umbenennen von " << destFilename << " nach " << newDestFilename;
    //    throw std::runtime_error(os.str());
    //}
}
#else
void moveFileToBackup(const string& destFilename)
{
    int i=1;
    string newDestFilename;
    do {
        std::ostringstream os;
        os << destFilename << ".bak." << i;
        newDestFilename=os.str();
        ++i;
    } while(util::fileExists(newDestFilename));
    int result=rename(destFilename.c_str(), newDestFilename.c_str());
    if (result!=0)    {
        std::ostringstream os;
        os << "Fehler: " << result << ", beim Umbenennen von " << destFilename << " nach " << newDestFilename;
        throw std::runtime_error(os.str());
    }
}
#endif

#ifdef _WIN32
void SyncFile::copySourceToDest(const string& source, const string& destination)
{
	TRACE("SyncFile::copySourceToDest source: ", source, " destination: ", destination);
    //Synchronize::setProgressMessage("Kopiere \n" + source + "\nzu\n"
    //                                + destination + "...");
    //QCoreApplication::processEvents();
    const auto src=source;
    const auto dest=destination;
    if (util::fileExists(destination)) {
        SetFileAttributes(util::string_to_wstring(dest).c_str(), FILE_ATTRIBUTE_NORMAL);
        if (si_.fatherSonPrinciple())
            moveFileToBackup(util::string_to_wstring(destination));
    }
    if (!CopyFile(util::string_to_wstring(src).c_str(), util::string_to_wstring(dest).c_str(), FALSE)) {
        util::handleLastError(string("kopiere \"" + source + "\" nach \""
                               + destination + string("\"")).c_str());
        return;
    }
	cout << util::currentDateTime() << ": kopiere \"" << source << " nach " << destination << '\n';

    ++copyCount_;
    if (si_.delAftercopy())
        DeleteFile(util::string_to_wstring(src).c_str());
}

void SyncFile::copyDestToSource(const string& destination, const string& source)
{
	TRACE("SyncFile::copyDestToSource source: ", source, " destination: ", destination);
    const string src=source;
    const string dest=destination;
    if (util::fileExists(source))
        SetFileAttributes(util::string_to_wstring(src).c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!CopyFile(util::string_to_wstring(dest).c_str(), util::string_to_wstring(src).c_str(), FALSE)) {
        util::handleLastError(string("kopiere \"" + destination + "\" nach \""
                               + source + string("\"")).c_str());
        return;
    }
	cout << util::currentDateTime() << ": kopiere \"" << destination << " nach " << source << '\n';
    ++copyCount_;
}
#else

template <typename TP>
std::time_t to_time_t(TP tp)
{
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
              + system_clock::now());
    return system_clock::to_time_t(sctp);
}

void SyncFile::copySourceToDest(const string& source, const string& destination)
{
    INFO("SyncFile::copySourceToDest source: ", source, " destination: ", destination);
    const string src=source;
    const string dest=destination;
    if (util::fileExists(destination)) {
        //chmod(dest.c_str(), S_IRWXU);
        std::filesystem::permissions(dest, fs::perms::owner_all);
        if (si_.fatherSonPrinciple())
            moveFileToBackup(destination);
    }
    try {
        //auto lwt=fs::   last_write_time(src);
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
        //util::copyFile(src, dest);

        //std::time_t cftime = to_time_t(lwt);
        //std::cout << "File write time is " << std::asctime(std::localtime(&cftime)) << '\n';
        //fs::last_write_time(dest, lwt);
        //cftime = to_time_t(lwt);
        //std::cout << "File write time is " << std::asctime(std::localtime(&cftime)) << '\n';
    }
    catch(std::exception& ex ) {
        util::handleLastError(string(string(ex.what()) + "kopiere \"" + source + "\" nach \"" + destination + string("\"")).c_str());
        return;
    }
    cout << util::currentDateTime() << ": kopiere \"" << source << " nach " << destination << '\n';
    ++copyCount_;
    if (si_.delAftercopy())
        remove(src.c_str());
}

void SyncFile::copyDestToSource(const string& destination, const string& source)
{
    INFO("SyncFile::copyDestToSource source: ", source, " destination: ", destination);
    const string src=source;
    const string dest=destination;
    if (util::fileExists(source))
        //chmod(src.c_str(), S_IRWXU);
	std::filesystem::permissions(src, fs::perms::owner_all);
    try {
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
    }
    catch(std::exception& ex ) {
        util::handleLastError(string(string(ex.what()) + "kopiere \"" + destination + "\" nach \"" + source + string("\"")).c_str());
        return;
    }
    cout << util::currentDateTime() << ": kopiere \"" << destination << " nach " << source << '\n';
    ++copyCount_;
}
#endif

void SyncFile::operator()()
{
	TRACE("SyncFile::operator()() source: ", si_.source(), " target: ", si_.target(), " wildcard: ", si_.wildcard());
    //QCoreApplication::processEvents();
    if (Synchronize::stopped())
        return;
    const string source=si_.source();
    string destination=si_.target();
    if (transferFilename_)
        transferFilename(source, destination);
#ifdef WIN32
	FileDifference fd(util::string_to_wstring(source), util::string_to_wstring(destination), si_.compareItems());
#else
    FileDifference fd(source, destination, si_.compareItems());
#endif
    fd();
    if (fd.result()==FileDifference::DestNotExists || fd.result()==FileDifference::SourceModified
        || (fd.result()==FileDifference::DestModified && si_.copyWhenTargetNewer()))
        copySourceToDest(source, destination);
    else if (si_.twoWaySync() && (fd.result()==FileDifference::SourceNotExits
                                  || fd.result()==FileDifference::DestModified))
        copyDestToSource(destination, source);
}

