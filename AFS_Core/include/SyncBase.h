#pragma once
#include "SyncItem.h"
#include <stdexcept>
#include <filesystem>
#include <set>
#include "Util.h"

namespace AFS_Core {

enum class CopyResult { Undefined, OK, Error, Stopped };

class SyncBase {
protected:
	// Owned copy, not a reference: sync operations can run long enough (large file
	// copies) that a live reference into Config's syncitems_ vector could be
	// invalidated by a concurrent GUI-thread add()/remove()/erase().
	const SyncItem sync_item_;
	std::size_t&	copy_count_;
	const bool		only_count_;
	SSHPATH			ssh_ {SSHPATH::none};

	void create_target_dir_if_not_exist(const std::wstring& targetdir = L"");
	void sync_dir_entry(const std::filesystem::directory_entry& dir_entry, const std::wstring& wildcard, const std::wstring& destination);
    bool drive_not_exist(const std::wstring& pathname, bool ssh);
	// Files that exist only in the target directory (one level, not recursive)
	// never reach FileDifference at all otherwise: SyncDir/SyncTree only ever
	// enumerate the source side, so a file removed from source (delete_in_target)
	// or one that appeared only in the target (two_way_sync) was never even
	// looked at. sourceNames is every filename seen in this same directory
	// level's source-side pass (matched by wildcard or not - either way it is
	// not "missing"), so this only acts on names absent from it. Reuses
	// SyncFile's existing FileDifference-based decision (delete vs. copy back)
	// by constructing it with a source path that is confirmed not to exist.
	void sync_target_only_entries(const std::wstring& source_dir, const std::wstring& target_dir,
		const std::wstring& wildcard, const std::set<std::wstring>& source_names);
protected:
	SyncBase() = delete;
	bool stop_requested() const;
public:
	SyncBase(const SyncItem& sync_item, std::size_t& copy_count, bool only_count);
	virtual void operator()() = 0;
	virtual ~SyncBase() = default;
};

class SyncException : public std::runtime_error {
	std::wstring message_;
public:
	// what() now carries the real message (UTF-8-encoded), not the hardcoded
	// "Fehler" placeholder it used to pass to runtime_error's constructor -
	// every catch(...)/catch(std::exception&) site logging ex.what() (there
	// is no dedicated catch(SyncException&) anywhere) silently dropped the
	// actual diagnostic (which file, which underlying OS error) the throw
	// site had already built.
	SyncException(const std::wstring& msg) : std::runtime_error(util::wstring_to_string(msg)), message_(msg) {};
	~SyncException() throw() {};

	std::wstring get_message() { return message_; }
};

std::wstring root_name(const std::wstring& path);
std::wstring without_volume_spec(const std::wstring& path);

} // namespace AFS_Core
