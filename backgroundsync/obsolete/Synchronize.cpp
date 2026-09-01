#include "stdafx.h"
#include "Synchronize.h"
#ifndef _UNIX
  #include <Windows.h>
  #include <wincon.h>
  #include <direct.h>
#else
  #include <dirent.h>
#endif


#include <string>
#include <util.h>
#include <winutil.h>
#include <filedifference.h>
#include <config.h>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <Synchronize.h>
#include <syncfile.h>
#include <delete_files.h>
#include "logging.h"

using namespace util;
using std::cout;
using std::cerr;

namespace fs=std::filesystem;

using std::string;
using std::wstring;
using util::withTrailingChar;

bool excluded(const string& fname, const Exclusions& e)
{
	if (e.empty())
		return false;
	const string excl = *e.begin();
    string::size_type i = 0;
	do {
		if (excl.size() <= i || fname.size() <= i)
			break;
			
		if (excl[i] == '*')
			return true;
		if (excl[i] != '?' && excl[i] != fname[i])
			return false;
		++i;
	} while (true);
	return true;
}

#ifdef _WIN32

void syncDir(const SyncItem& si, std::size_t& copyCount)
{
	TRACE("syncDir source: ", si.source(), " target: ", si.target(), " wildcard: ", si.wildcard());
    string source=si.source();
    if (source.empty())
        return;
    if (*source.rbegin() != L'/')
        source += '/';
    if (si.wildcard().empty())
        source += "*.*";
    else
        source += si.wildcard();
    string destination=si.target();
	if (!util::fileExists(destination))
		_mkdir(destination.c_str());
		
    const std::string src=source;
    WIN32_FIND_DATA FData;
	HANDLE hSearch(FindFirstFile(string_to_wstring(src).c_str(), &FData));
    BOOL MoreFiles = FALSE;
    if (hSearch == INVALID_HANDLE_VALUE) return;
    string dir=source.substr(0, source.find_last_of('/'));
    do 	{
        if ((FData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            const string fname= dir + "/" + wstring_to_string(FData.cFileName);
            const string dest=util::withTrailingChar(destination, '/')+ wstring_to_string(FData.cFileName);
			if (!excluded(wstring_to_string(FData.cFileName), si.exclusions()))
				SyncFile(SyncItem(si.typeStr(), fname, dest, "", si.exclusions(), si.flags(), si.compareItems()), copyCount)();
        }
        MoreFiles = FindNextFile(hSearch,&FData);
    } while (MoreFiles && !Synchronize::stopped());
	FindClose(hSearch);
}
#else
void syncDir(const SyncItem& si, std::size_t& copyCount)
{
    string source=si.source();
    if (source.empty())
        return;
    if (*source.rbegin() != '/')
        source += '/';
//    if (si.wildcard().empty())
//        source += "*.*";
//    else
//        source += si.wildcard();
    string destination=si.target();

    if (!fs::exists(destination))
        if (!fs::create_directories(destination)) {
            cerr << "Cannot create: " << destination << "\n";
            return;
        }
    //if (!util::dirExists(destination.c_str()))
    //    util::createDir(destination.c_str());

    const std::string src=source;
    const string dir=source.substr(0, source.find_last_of('/'));
    if(fs::is_directory(src)) {
        for(auto& p: fs::directory_iterator(src)) {
            const string filename=p.path().filename();
            const string src= dir + "/" + filename;
            const string dest=util::withTrailingChar(destination, '/')+filename;
            Exclusions excl;
            SyncFile(SyncItem(si.typeStr(), src, dest, "", si.exclusions(), si.flags(), si.compareItems()), copyCount)();
        }
     }
//    if (util::dirExists(dir.c_str())) {
//        DIR  *dirHandle =  opendir(dir.c_str());
//        struct dirent *dirEntry;
//        if (dirHandle != NULL ) {
//                while ((dirEntry = readdir(dirHandle)) != 0)
//                {
//                    const string filename=dirEntry->d_name;
//                    const string src= dir + "/" + filename;
//                    const string dest=util::withTrailingChar(destination, '/')+filename;
//                    SyncFile(SyncItem(si.typeStr(), src, dest, "", si.flags(), si.compareItems()), copyCount)();
//                }
//                closedir( dirHandle );
//            }
//     }
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class SyncTree
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SyncTree {
    const SyncItem& syncItem_;
    std::size_t&    copyCount_;
public:
    SyncTree(const SyncItem& si, std::size_t& copyCount): syncItem_(si), copyCount_(copyCount) {}
    void operator()(const string& sourcename, const string& targetname, const string& wildcard);
protected:
    void syncBranch(const string& sourcename, const string& targetname, const string& wildcard);
};

void SyncTree::operator()(const string& sourcename, const string& targetname, const string& wildcard)
{
    syncBranch(sourcename, targetname, wildcard);
}

#ifdef _WIN32
void SyncTree::syncBranch(const string& sourcename, const string& targetname, const string& wildcard)
{
	TRACE("syncBranch source: ", sourcename, " target: ", targetname, " wildcard: ", wildcard);
    if (Synchronize::stopped())
        return;
	syncDir(SyncItem("Verzeichnis",  sourcename, targetname, wildcard, syncItem_.exclusions(), syncItem_.flags(),
                     syncItem_.compareItems()), copyCount_);
    WIN32_FIND_DATA FData;
    string search = withTrailingChar(sourcename, L'/') + "*.*";
	HANDLE  hSearch(FindFirstFile(string_to_wstring(search).c_str(), &FData));
    BOOL MoreFiles = FALSE;
	if (hSearch == INVALID_HANDLE_VALUE) {
		FindClose(hSearch);
		return;
	}
    do 	{
        const string fname= wstring_to_string(FData.cFileName);
        const string source=withTrailingChar(sourcename, L'/') + fname;
        const string target=withTrailingChar(targetname, L'/') + fname;
        if ((FData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && fname != "." && fname != "..") {
            syncBranch(source, target, wildcard);
        }
        MoreFiles = FindNextFile(hSearch,&FData);
    } while (MoreFiles && !Synchronize::stopped());
	FindClose(hSearch);
}
#else
void SyncTree::syncBranch(const string& sourcename, const string& targetname, const string& wildcard)
{
    if (Synchronize::stopped())
        return;
    syncDir(SyncItem("Verzeichnis",  sourcename, targetname, wildcard, syncItem_.exclusions(), syncItem_.flags(),
                     syncItem_.compareItems()), copyCount_);
   for(auto& p: fs::directory_iterator(sourcename)) {
        if (Synchronize::stopped())
            return;
         const string fname= p.path().filename();
         const string source=withTrailingChar(sourcename, '/') + fname;
         const string target=withTrailingChar(targetname, '/') + fname;
         if (fs::is_directory(source)/* && fname != "." && fname != ".."*/) {
             syncBranch(source, target, wildcard);
         } else {
             Exclusions excl;
             SyncFile(SyncItem("", source, target, "", excl, SyncFlags(), CompareItems()), copyCount_)();
         }

    }

//    DIR  *dirHandle =  opendir(sourcename.c_str());
//    struct dirent *dirEntry;
//    if (dirHandle != NULL ) {
//        while ((dirEntry = readdir(dirHandle)) != 0) {
//            if (Synchronize::stopped())
//                return;

//            const string fname=dirEntry->d_name;
//            const string source=withTrailingChar(sourcename, '/') + fname;
//            const string target=withTrailingChar(targetname, '/') + fname;
//            if (dirEntry->d_type == DT_DIR && fname != "." && fname != "..") {
//                syncBranch(source, target, wildcard);
//            }
        }
//        closedir( dirHandle );
//    }
//}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class Synchronize
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ProgressDialog* Synchronize::progressDialog_ = nullptr;
bool Synchronize::stop_=false;

Synchronize::Synchronize(const Config& c)
    : config_(c)
{}

void Synchronize::stop()
{
   stop_=true;
}

bool Synchronize::stopped()
{
    return stop_;
}

void Synchronize::setProgressDialog(ProgressDialog* pd)
{
    progressDialog_=pd;
}

void Synchronize::setProgressMessage(const std::string& )
{
    //if (progressDialog_==nullptr)
    //    return;
    //progressDialog_->setMessage(m);
}

void Synchronize::operator()()
{
    stop_=false;
    for (auto i: config_.syncitems()) {
        if (!i.enabled())
            continue;
        if (i.typeStr()=="Verzeichnis" || i.typeStr()=="Verzeichnis mit Wildcard" || i.typeStr() == "Verzeichnis mit Wildcard und Ausnahmen")
            syncDir(i, copyCount_);
        else if (i.typeStr()=="Datei")
            SyncFile(i, copyCount_, true)();
        else if (i.typeStr()=="Verzeichnisbaum" || i.typeStr()=="Verzeichnisbaum mit Wildcard") {
            SyncTree sync(i, copyCount_);
            sync(i.source(), i.target(), i.wildcard());
        }
		else if (i.typeStr() == "Loesche Dateien mit Wildcard") {
			DeleteFiles sync(i);
			sync();
		}
    }
}
