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
	const SyncItem sync_item_;  // [SyncBase.md#sync_item_]
	std::size_t&	copy_count_;
	const bool		only_count_;
	SSHPATH			ssh_ {SSHPATH::none};

	void create_target_dir_if_not_exist(const std::wstring& targetdir = L"");
	void sync_dir_entry(const std::filesystem::directory_entry& dir_entry, const std::wstring& wildcard, const std::wstring& destination);
	void sync_file(const std::wstring& source, const std::wstring& dest, const std::wstring& wildcard);  // [SyncBase.md#sync_file]
    bool drive_not_exist(const std::wstring& pathname, bool ssh);
	void sync_target_only_entries(const std::wstring& source_dir, const std::wstring& target_dir,
		const std::wstring& wildcard, const std::set<std::wstring>& source_names);  // [SyncBase.md#sync_target_only_entries]
	bool stop_requested() const;
	SyncBase() = delete;
public:
	SyncBase(const SyncItem& sync_item, std::size_t& copy_count, bool only_count);
	virtual void operator()() = 0;
	virtual ~SyncBase() = default;
};

class SyncException : public std::runtime_error {  // [SyncBase.md#SyncException]
	std::wstring message_;
public:
	explicit SyncException(const std::wstring& msg) : std::runtime_error(util::wstring_to_string(msg)), message_(msg) {}
	std::wstring get_message() const { return message_; }
};

std::wstring root_name(const std::wstring& path);
std::wstring without_volume_spec(const std::wstring& path);

} // namespace AFS_Core
