#include "pch.h"
#include <strutil.h>

using namespace AFS_Core;

using util::string_to_wstring;
using util::wstring_to_string;

// Regression test: wstring_to_string() used to truncate/corrupt non-ASCII
// characters instead of UTF-8-encoding them, so anything with umlauts (SSH
// paths, host/user/password, filenames) got silently mangled on Linux/macOS.
TEST(StrUtilTests, WstringToStringRoundTripsUmlauts)
{
    const std::wstring original = L"Prüfung äöüÄÖÜß häckchen";

    const std::string utf8 = wstring_to_string(original);
    const std::wstring roundTripped = string_to_wstring(utf8);

    EXPECT_EQ(roundTripped, original);
}

TEST(StrUtilTests, WstringToStringProducesValidUtf8ForUmlauts)
{
    const std::wstring original = L"ü";

    const std::string utf8 = wstring_to_string(original);

    // U+00FC (ü) is encoded in UTF-8 as the two bytes 0xC3 0xBC.
    ASSERT_EQ(utf8.size(), 2u);
    EXPECT_EQ(static_cast<unsigned char>(utf8[0]), 0xC3);
    EXPECT_EQ(static_cast<unsigned char>(utf8[1]), 0xBC);
}

TEST(StrUtilTests, WstringToStringRoundTripsPlainAscii)
{
    const std::wstring original = L"plain-ascii_123";

    EXPECT_EQ(string_to_wstring(wstring_to_string(original)), original);
}

#include <util.h>

// Regression test (Fehler 4, round 4): withTrailingChar() dereferenced
// rbegin() without checking for an empty string - undefined behavior (a hard
// iterator-debug assert on MSVC). Reachable from every path-joining call site
// whenever a source/target ends up empty (bad config entry, mangled path).
TEST(StrUtilTests, WithTrailingCharHandlesEmptyString)
{
    EXPECT_EQ(util::withTrailingChar(L"", L'/'), L"/");
}

TEST(StrUtilTests, WithTrailingCharKeepsExistingBehavior)
{
    EXPECT_EQ(util::withTrailingChar(L"/a", L'/'), L"/a/");
    EXPECT_EQ(util::withTrailingChar(L"/a/", L'/'), L"/a/");
}

// Fehler 2 (round 5): wildcardToRegex() only replaced * and ? but escaped
// nothing else, so the remaining characters were interpreted as regex:
// "*.txt" (-> ".*.txt") matched "abXtxt", parentheses became groups so
// "Fotos (2020)*" never matched its own files, and an unbalanced "[" threw
// std::regex_error on every comparison, silently aborting the directory sync.
TEST(StrUtilTests, WildmatchTreatsNonWildcardCharsLiterally)
{
    EXPECT_TRUE(util::wildmatch(L"a.txt", L"*.txt"));
    EXPECT_FALSE(util::wildmatch(L"abXtxt", L"*.txt"));
    EXPECT_TRUE(util::wildmatch(L"Fotos (2020) Juli", L"Fotos (2020)*"));
    EXPECT_TRUE(util::wildmatch(L"data[1].txt", L"data[1].txt"));
    EXPECT_NO_THROW(util::wildmatch(L"x", L"data["));
    EXPECT_TRUE(util::wildmatch(L"a+b.txt", L"a+b.txt"));
    EXPECT_TRUE(util::wildmatch(L"file1.txt", L"file?.txt")); // ? still works
}

// Fehler 5 (round 6): remove_file() discarded fs::remove()'s own return value
// and always reported success regardless of whether anything was actually
// deleted - e.g. for an SSH "account:/path" string misinterpreted as a bogus
// local path, or any other path that simply didn't exist.
TEST(StrUtilTests, RemoveFileReturnsWhetherSomethingWasActuallyRemoved)
{
    const std::wstring nonExistent = L"/nonexistent_afx_dir/nonexistent_afx_file.txt";
    EXPECT_FALSE(util::remove_file(nonExistent, LogLevel::None));
}
