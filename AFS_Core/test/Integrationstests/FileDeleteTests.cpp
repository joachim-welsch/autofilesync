#include "pch.h"
#include "FileDelete.h"
#include "Config.h"
#include <RecycleBin.h>
#include <filesystem>
#include <fstream>
#include <regex>

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {

class FileDeleteTest : public ::testing::Test {
protected:
    fs::path root_;
    fs::path bin_;

    void SetUp() override {
        root_ = fs::temp_directory_path() / "filedelete_test";
        fs::remove_all(root_);
        fs::create_directories(root_ / "sub");
        bin_ = root_ / "bin";
        RecycleBin::instance().bin_path(bin_);
        Config::instance().settings().delete_mode(DeleteMode::RecycleBin);
    }

    void TearDown() override {
        Config::instance().reset();
        fs::remove_all(root_);
    }

    fs::path create_file(const fs::path& relative, const std::string& content = "x") const {
        const fs::path pathname = root_ / relative;
        fs::create_directories(pathname.parent_path());
        std::ofstream os(pathname);
        os << content;
        return pathname;
    }

    static bool has_timestamp_prefix(const fs::path& item, const std::wstring& original_name) {
        return std::regex_match(item.filename().wstring(),
            std::wregex(LR"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-\d{3}_)" + original_name));
    }
};

TEST_F(FileDeleteTest, RecycleBinModeMovesFileIntoBin)
{
    const fs::path source = create_file("sub/data.txt", "content");

    EXPECT_TRUE(delete_file(source.wstring()));

    EXPECT_FALSE(fs::exists(source));
    const RecycleBin::ItemList items = RecycleBin::instance().list_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(has_timestamp_prefix(items[0], L"data\\.txt"));
}

// The bin mirrors the origin below itself, so a restore can tell where the file
// came from - and two files of the same name from different directories do not
// land on top of each other.
TEST_F(FileDeleteTest, RecycleBinModeMirrorsTheOriginDirectory)
{
    const fs::path source = create_file("sub/data.txt");

    ASSERT_TRUE(delete_file(source.wstring()));

    const RecycleBin::ItemList items = RecycleBin::instance().list_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].parent_path(), source.parent_path().relative_path());
}

TEST_F(FileDeleteTest, RecycleBinModeKeepsFilesOfTheSameName)
{
    const fs::path first = create_file("sub/data.txt", "first");
    const fs::path second = create_file("other/data.txt", "second");

    ASSERT_TRUE(delete_file(first.wstring()));
    ASSERT_TRUE(delete_file(second.wstring()));

    EXPECT_EQ(RecycleBin::instance().list_items().size(), 2u);
}

TEST_F(FileDeleteTest, PermanentModeDeletesWithoutTouchingTheBin)
{
    Config::instance().settings().delete_mode(DeleteMode::Permanent);
    const fs::path source = create_file("sub/data.txt");

    EXPECT_TRUE(delete_file(source.wstring()));

    EXPECT_FALSE(fs::exists(source));
    EXPECT_TRUE(RecycleBin::instance().list_items().empty());
}

TEST_F(FileDeleteTest, MissingFileFailsInBothModes)
{
    const fs::path missing = root_ / "sub" / "gone.txt";

    EXPECT_FALSE(delete_file(missing.wstring()));
    Config::instance().settings().delete_mode(DeleteMode::Permanent);
    EXPECT_FALSE(delete_file(missing.wstring()));
}

TEST_F(FileDeleteTest, RecycleBinModeMovesDirectories)
{
    create_file("sub/tree/inner.txt");

    EXPECT_TRUE(delete_file((root_ / "sub" / "tree").wstring()));

    EXPECT_FALSE(fs::exists(root_ / "sub" / "tree"));
    const RecycleBin::ItemList items = RecycleBin::instance().list_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].filename(), fs::path(L"inner.txt"));
}

} // namespace
