#pragma once
#include "SyncItem.h"
#include <stdexcept>
#include <filesystem>
#include <set>
#include "util.h"

enum class CopyResult { Undefined, OK, Error, Stopped };

class SyncBase {
protected:
	// Owned copy, not a reference: sync operations can run long enough (large file
	// copies) that a live reference into Config's syncitems_ vector could be
	// invalidated by a concurrent GUI-thread add()/remove()/erase().
	const SyncItem syncItem_;
	std::size_t&	copyCount_;
	const bool		onlyCount_;
	SSHPATH			ssh_ {SSHPATH::none};

	void createTargetDirIfNotExist(const std::wstring& targetdir = L"");
	void syncDirEntry(const std::filesystem::directory_entry& dir_entry, const std::wstring& wildcard, const std::wstring& destination);
    bool driveNotExist(const std::wstring& pathname, bool ssh);
	// Files that exist only in the target directory (one level, not recursive)
	// never reach FileDifference at all otherwise: SyncDir/SyncTree only ever
	// enumerate the source side, so a file removed from source (deleteInTarget)
	// or one that appeared only in the target (twoWaySync) was never even
	// looked at. sourceNames is every filename seen in this same directory
	// level's source-side pass (matched by wildcard or not - either way it is
	// not "missing"), so this only acts on names absent from it. Reuses
	// SyncFile's existing FileDifference-based decision (delete vs. copy back)
	// by constructing it with a source path that is confirmed not to exist.
	void syncTargetOnlyEntries(const std::wstring& sourceDir, const std::wstring& targetDir,
		const std::wstring& wildcard, const std::set<std::wstring>& sourceNames);
protected:
	SyncBase() = delete;
	bool stopRequested() const;
public:
	SyncBase(const SyncItem& syncItem, std::size_t& copyCount, bool onlyCount);
	virtual void operator()() = 0;
	virtual ~SyncBase() = default;
};

class SyncException : public std::runtime_error {
	std::wstring message;
public:
	// what() now carries the real message (UTF-8-encoded), not the hardcoded
	// "Fehler" placeholder it used to pass to runtime_error's constructor -
	// every catch(...)/catch(std::exception&) site logging ex.what() (there
	// is no dedicated catch(SyncException&) anywhere) silently dropped the
	// actual diagnostic (which file, which underlying OS error) the throw
	// site had already built.
	SyncException(const std::wstring& msg) : std::runtime_error(util::wstring_to_string(msg)), message(msg) {};
	~SyncException() throw() {};

	std::wstring get_message() { return message; }
};

std::wstring rootName(const std::wstring& path);
std::wstring withoutVolumeSpec(const std::wstring& path);


//################################################
