#pragma once
#include <filesystem>
#include <mutex>
#include <vector>

namespace AFS_Core {

class RecycleBin {  // [RecycleBin.md#RecycleBin]
public:
	using ItemList = std::vector<std::filesystem::path>;
private:
	mutable std::mutex mutex_;
	std::filesystem::path path_;
	RecycleBin();
	~RecycleBin();
	RecycleBin(const RecycleBin&) = delete;
	RecycleBin& operator=(const RecycleBin&) = delete;
	bool check_relative(const std::filesystem::path& item) const;  // the private helpers below assume a held mutex_
	bool move(const std::filesystem::path& from, const std::filesystem::path& to) const;
public:
	static RecycleBin& instance();
	void bin_path(const std::filesystem::path& path);
	std::filesystem::path bin_path() const;
	bool add_item(const std::filesystem::path& item);
	bool add_item(const std::filesystem::path& source, const std::filesystem::path& item);  // [RecycleBin.md#add_item-outside-cwd]
	bool restore_item(const std::filesystem::path& item);
	ItemList list_items() const;  // by value: a reference would outlive the lock that protects the bin
};

} // namespace AFS_Core
