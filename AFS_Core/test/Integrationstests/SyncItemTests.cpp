#include "pch.h"
#include "syncitem.h"

// Exclusions feature: isExcluded() reuses the same */? wildcard syntax the
// existing wildcard field already uses (via util::wildmatch()), matched
// against the entry's own name (not a full path) - consistent with how the
// wildcard field itself is applied throughout SyncDir/SyncTree.
TEST(SyncItemTests, IsExcludedMatchesAnyPatternInTheList)
{
    Exclusions exclusions;
    exclusions.push_back(L"*.tmp");
    exclusions.push_back(L"node_modules");
    exclusions.push_back(L".git");

    EXPECT_TRUE(isExcluded(L"cache.tmp", exclusions));
    EXPECT_TRUE(isExcluded(L"node_modules", exclusions));
    EXPECT_TRUE(isExcluded(L".git", exclusions));
    EXPECT_FALSE(isExcluded(L"readme.txt", exclusions));
    EXPECT_FALSE(isExcluded(L"my_node_modules_backup.txt", exclusions)); // not an exact/wildcard match
}

TEST(SyncItemTests, IsExcludedWithEmptyListExcludesNothing)
{
    Exclusions exclusions;
    EXPECT_FALSE(isExcluded(L"anything.txt", exclusions));
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
