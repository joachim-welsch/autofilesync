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
	// Synchronises one file of the item; only counts it when only_count_ is set.
	// Every failure is logged and reported here, so one bad file never stops
	// the directory it lives in.
	void sync_file(const std::wstring& source, const std::wstring& dest, const std::wstring& wildcard);
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
	bool stop_requested() const;
	SyncBase() = delete;
public:
	SyncBase(const SyncItem& sync_item, std::size_t& copy_count, bool only_count);
	virtual void operator()() = 0;
	virtual ~SyncBase() = default;
};

// what() carries the UTF-8 encoded message, so the catch(std::exception&)
// handlers see the same diagnostic as get_message().
class SyncException : public std::runtime_error {
	std::wstring message_;
public:
	explicit SyncException(const std::wstring& msg) : std::runtime_error(util::wstring_to_string(msg)), message_(msg) {}
	std::wstring get_message() const { return message_; }
};

std::wstring root_name(const std::wstring& path);
std::wstring without_volume_spec(const std::wstring& path);

} // namespace AFS_Core
