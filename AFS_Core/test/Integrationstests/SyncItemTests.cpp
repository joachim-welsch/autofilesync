#include "pch.h"
#include "SyncItem.h"

using namespace AFS_Core;  // [SyncItemTests.md#exclusions-wildcard-syntax]

TEST(SyncItemTests, IsExcludedMatchesAnyPatternInTheList)
{
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    exclusions.push_back(L"node_modules");
    exclusions.push_back(L".git");

    EXPECT_TRUE(is_excluded(L"cache.tmp", exclusions));
    EXPECT_TRUE(is_excluded(L"node_modules", exclusions));
    EXPECT_TRUE(is_excluded(L".git", exclusions));
    EXPECT_FALSE(is_excluded(L"readme.txt", exclusions));
    EXPECT_FALSE(is_excluded(L"my_node_modules_backup.txt", exclusions)); // not an exact/wildcard match
}

TEST(SyncItemTests, IsExcludedWithEmptyListExcludesNothing)
{
    Exclusions exclusions;
    EXPECT_FALSE(is_excluded(L"anything.txt", exclusions));
}

TEST(SyncItemTests, SyncItemStoresAndCopiesExclusions)  // [SyncItemTests.md#SyncItemStoresAndCopiesExclusions]
{
    Exclusions exclusions;
    exclusions.push_back(L"*.log");
    SyncItem si;
    si.exclusions(exclusions);

    SyncItem copy(si);
    ASSERT_EQ(copy.exclusions().size(), 1u);
    EXPECT_EQ(copy.exclusions()[0], L"*.log");
}

TEST(SyncItemTests, SyncItemTypeKeyStaysUntranslated)  // [SyncItemTests.md#SyncItemTypeKeyStaysUntranslated]
{
    EXPECT_EQ(sync_item_type_key(SyncItemType::File), L"Datei");
    EXPECT_EQ(sync_item_type_key(SyncItemType::Dir), L"Verzeichnis");
    EXPECT_EQ(sync_item_type_key(SyncItemType::Tree), L"Verzeichnisbaum");
    EXPECT_EQ(sync_item_type_key(SyncItemType::Undefined), L"");
}

TEST(SyncItemTests, Str2SyncItemTypeAcceptsKeyAndTranslation)
{
    for (auto type : {SyncItemType::File, SyncItemType::Dir, SyncItemType::Tree}) {
        EXPECT_EQ(str_to_sync_item_type(sync_item_type_key(type)), type);
        EXPECT_EQ(str_to_sync_item_type(sync_item_type_to_str(type)), type);
    }
    EXPECT_EQ(str_to_sync_item_type(L"kein Typ"), SyncItemType::Undefined);
}
