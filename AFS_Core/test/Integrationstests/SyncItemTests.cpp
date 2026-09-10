#include "pch.h"
#include "SyncItem.h"

// Exclusions feature: is_excluded() reuses the same */? wildcard syntax the
// existing wildcard field already uses (via util::wildmatch()), matched
// against the entry's own name (not a full path) - consistent with how the
// wildcard field itself is applied throughout SyncDir/SyncTree.

using namespace AFS_Core;

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

// SyncItem persists/copies its exclusion list like any other field.
TEST(SyncItemTests, SyncItemStoresAndCopiesExclusions)
{
    Exclusions exclusions;
    exclusions.push_back(L"*.log");
    SyncItem si;
    si.exclusions(exclusions);

    SyncItem copy(si);
    ASSERT_EQ(copy.exclusions().size(), 1u);
    EXPECT_EQ(copy.exclusions()[0], L"*.log");
}

// sync_item_type_to_str() is translated (GNU gettext), sync_item_type_key() is not:
// the configuration file must stay readable no matter which language wrote it.
TEST(SyncItemTests, SyncItemTypeKeyStaysUntranslated)
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
