#pragma once
#include <filesystem>
#include <mutex>
#include <vector>

namespace AFS_Core {

// Items are addressed by a path relative to the current working directory. The
// path is mirrored inside the bin, so restoreItem() can put the file back where
// it came from.
class RecycleBin {
public:
	using ItemList = std::vector<std::filesystem::path>;
private:
	mutable std::mutex mutex_;
	std::filesystem::path path_;
	RecycleBin();
	~RecycleBin();
	RecycleBin(const RecycleBin&) = delete;
	RecycleBin& operator=(const RecycleBin&) = delete;
	// The helpers below assume the caller already holds mutex_.
	bool checkRelative(const std::filesystem::path& item) const;
	bool move(const std::filesystem::path& from, const std::filesystem::path& to) const;
public:
	static RecycleBin& instance();
	void binPath(const std::filesystem::path& path);
	std::filesystem::path binPath() const;
	bool addItem(const std::filesystem::path& item);
	// Same, but for a source that lies outside the current working directory:
	// it is stored in the bin under the relative path 'item', which lets the
	// caller pick the name there (a timestamp prefix, say).
	bool addItem(const std::filesystem::path& source, const std::filesystem::path& item);
	bool restoreItem(const std::filesystem::path& item);
	// By value: a reference would outlive the lock that protects the bin.
	ItemList listItems() const;
};

} // namespace AFS_Core
