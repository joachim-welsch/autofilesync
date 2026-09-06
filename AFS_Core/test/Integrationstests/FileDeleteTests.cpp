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
        RecycleBin::instance().binPath(bin_);
        Config::instance().settings().deleteMode(DeleteMode::RecycleBin);
    }

    void TearDown() override {
        Config::instance().reset();
        fs::remove_all(root_);
    }

    fs::path createFile(const fs::path& relative, const std::string& content = "x") const {
        const fs::path pathname = root_ / relative;
        fs::create_directories(pathname.parent_path());
        std::ofstream os(pathname);
        os << content;
        return pathname;
    }

    static bool hasTimestampPrefix(const fs::path& item, const std::wstring& originalName) {
        return std::regex_match(item.filename().wstring(),
            std::wregex(LR"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-\d{3}_)" + originalName));
    }
};

TEST_F(FileDeleteTest, RecycleBinModeMovesFileIntoBin)
{
    const fs::path source = createFile("sub/data.txt", "content");

    EXPECT_TRUE(deleteFile(source.wstring()));

    EXPECT_FALSE(fs::exists(source));
    const RecycleBin::ItemList items = RecycleBin::instance().listItems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_TRUE(hasTimestampPrefix(items[0], L"data\\.txt"));
}

// The bin mirrors the origin below itself, so a restore can tell where the file
// came from - and two files of the same name from different directories do not
// land on top of each other.
TEST_F(FileDeleteTest, RecycleBinModeMirrorsTheOriginDirectory)
{
    const fs::path source = createFile("sub/data.txt");

    ASSERT_TRUE(deleteFile(source.wstring()));

    const RecycleBin::ItemList items = RecycleBin::instance().listItems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].parent_path(), source.parent_path().relative_path());
}

TEST_F(FileDeleteTest, RecycleBinModeKeepsFilesOfTheSameName)
{
    const fs::path first = createFile("sub/data.txt", "first");
    const fs::path second = createFile("other/data.txt", "second");

    ASSERT_TRUE(deleteFile(first.wstring()));
    ASSERT_TRUE(deleteFile(second.wstring()));

    EXPECT_EQ(RecycleBin::instance().listItems().size(), 2u);
}

TEST_F(FileDeleteTest, PermanentModeDeletesWithoutTouchingTheBin)
{
    Config::instance().settings().deleteMode(DeleteMode::Permanent);
    const fs::path source = createFile("sub/data.txt");

    EXPECT_TRUE(deleteFile(source.wstring()));

    EXPECT_FALSE(fs::exists(source));
    EXPECT_TRUE(RecycleBin::instance().listItems().empty());
}

TEST_F(FileDeleteTest, MissingFileFailsInBothModes)
{
    const fs::path missing = root_ / "sub" / "gone.txt";

    EXPECT_FALSE(deleteFile(missing.wstring()));
    Config::instance().settings().deleteMode(DeleteMode::Permanent);
    EXPECT_FALSE(deleteFile(missing.wstring()));
}

TEST_F(FileDeleteTest, RecycleBinModeMovesDirectories)
{
    createFile("sub/tree/inner.txt");

    EXPECT_TRUE(deleteFile((root_ / "sub" / "tree").wstring()));

    EXPECT_FALSE(fs::exists(root_ / "sub" / "tree"));
    const RecycleBin::ItemList items = RecycleBin::instance().listItems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].filename(), fs::path(L"inner.txt"));
}

} // namespace
