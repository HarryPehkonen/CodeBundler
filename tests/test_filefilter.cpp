#include "filefilter.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace codebundler;

namespace {

// Parse one rule, asserting that it parsed: a test that silently got an empty
// rule would pass for the wrong reason.
FilterRule rule(const std::string& text, const std::string& source = "--exclude")
{
    FilterRule parsed;
    EXPECT_TRUE(FileFilter::parseRule(text, source, parsed)) << "did not parse: '" << text << "'";
    return parsed;
}

bool excludedByOne(const std::string& pattern, const std::string& path)
{
    FileFilter filter({ rule(pattern) }, false);
    return !filter.keeps(path);
}

// A rule as the bundler builds one for --include: the flag, not a leading '!',
// decides the direction.
FilterRule includeRule(const std::string& pattern)
{
    FilterRule parsed = rule(pattern, "--include");
    parsed.include = true;
    return parsed;
}

bool includedByName(const std::string& pattern, const std::string& path)
{
    FileFilter filter({ includeRule(pattern) }, true);
    return filter.keeps(path);
}

} // namespace

//--------------------------------------------------------------------------
// Parsing
//--------------------------------------------------------------------------

TEST(FileFilterParseTest, PlainNameIsAnExcludeMatchingAnyComponent)
{
    const FilterRule parsed = rule("tests");
    EXPECT_EQ(parsed.pattern, "tests");
    EXPECT_FALSE(parsed.include);
    EXPECT_FALSE(parsed.directoryOnly);
    EXPECT_FALSE(parsed.rootAnchored);
    EXPECT_EQ(parsed.source, "--exclude");
    EXPECT_EQ(parsed.text, "tests");
}

TEST(FileFilterParseTest, ASlashAnchorsThePatternAtTheRoot)
{
    EXPECT_TRUE(rule("docs/PLAN.md").rootAnchored);
    EXPECT_FALSE(rule("PLAN.md").rootAnchored);
    EXPECT_TRUE(rule("/docs").rootAnchored);
    EXPECT_EQ(rule("/docs").pattern, "docs");
}

TEST(FileFilterParseTest, ATrailingSlashMeansDirectoriesOnly)
{
    const FilterRule parsed = rule("tests/");
    EXPECT_TRUE(parsed.directoryOnly);
    EXPECT_EQ(parsed.pattern, "tests");
}

TEST(FileFilterParseTest, ALeadingBangMakesItAnInclude)
{
    const FilterRule parsed = rule("!src/keep.cpp");
    EXPECT_TRUE(parsed.include);
    EXPECT_EQ(parsed.pattern, "src/keep.cpp");
    EXPECT_EQ(parsed.text, "!src/keep.cpp");
}

TEST(FileFilterParseTest, BlankLinesAndCommentsAreNotRules)
{
    FilterRule parsed;
    EXPECT_FALSE(FileFilter::parseRule("", "x", parsed));
    EXPECT_FALSE(FileFilter::parseRule("   \t ", "x", parsed));
    EXPECT_FALSE(FileFilter::parseRule("# a comment", "x", parsed));
    // A rule that names nothing after the marks is not a rule either.
    EXPECT_FALSE(FileFilter::parseRule("!", "x", parsed));
    EXPECT_FALSE(FileFilter::parseRule("/", "x", parsed));
}

//--------------------------------------------------------------------------
// Glob matching
//--------------------------------------------------------------------------

TEST(FileFilterGlobTest, AStarStopsAtASlash)
{
    EXPECT_TRUE(FileFilter::globMatches("*.cpp", "main.cpp"));
    EXPECT_FALSE(FileFilter::globMatches("*.cpp", "src/main.cpp"));
    EXPECT_TRUE(FileFilter::globMatches("src/*.cpp", "src/main.cpp"));
    EXPECT_FALSE(FileFilter::globMatches("src/*.cpp", "src/ai/main.cpp"));
}

TEST(FileFilterGlobTest, ADoubleStarCrossesSlashes)
{
    EXPECT_TRUE(FileFilter::globMatches("**/*.cpp", "src/ai/main.cpp"));
    EXPECT_TRUE(FileFilter::globMatches("docs/**", "docs/a/b.md"));
    EXPECT_TRUE(FileFilter::globMatches("**/tool", "tool")); // zero components
    EXPECT_TRUE(FileFilter::globMatches("**/tests/**", "a/tests/b/c.cpp"));
    EXPECT_FALSE(FileFilter::globMatches("docs/**", "docsx/a.md"));
}

TEST(FileFilterGlobTest, QuestionMarkIsOneCharacter)
{
    EXPECT_TRUE(FileFilter::globMatches("f?o", "foo"));
    EXPECT_FALSE(FileFilter::globMatches("f?o", "f/o"));
    EXPECT_FALSE(FileFilter::globMatches("f?o", "fo"));
}

TEST(FileFilterGlobTest, CharacterClassesIncludingNegation)
{
    EXPECT_TRUE(FileFilter::globMatches("[a-c]at", "bat"));
    EXPECT_FALSE(FileFilter::globMatches("[a-c]at", "dat"));
    EXPECT_TRUE(FileFilter::globMatches("[!a]at", "bat"));
    EXPECT_FALSE(FileFilter::globMatches("[!a]at", "aat"));
    EXPECT_TRUE(FileFilter::globMatches("[^a]at", "cat"));
    // An unterminated class is a literal '[', not a class.
    EXPECT_TRUE(FileFilter::globMatches("a[bc", "a[bc"));
    EXPECT_FALSE(FileFilter::globMatches("a[bc", "ab"));
}

TEST(FileFilterGlobTest, ABackslashEscapesTheNextCharacter)
{
    EXPECT_TRUE(FileFilter::globMatches("a\\*b", "a*b"));
    EXPECT_FALSE(FileFilter::globMatches("a\\*b", "axb"));
}

//--------------------------------------------------------------------------
// Selection
//--------------------------------------------------------------------------

TEST(FileFilterSelectionTest, ANameExcludesEveryDirectoryOfThatName)
{
    EXPECT_TRUE(excludedByOne("tests", "tests/test_main.cpp"));
    EXPECT_TRUE(excludedByOne("tests", "src/tests/test_main.cpp"));
    // Only a whole component counts: this is not a substring match.
    EXPECT_FALSE(excludedByOne("tests", "src/testsuite.cpp"));
    EXPECT_FALSE(excludedByOne("tests", "src/latest/t.cpp"));
}

TEST(FileFilterSelectionTest, ATrailingSlashSparesAFileOfThatName)
{
    // "tests/" prunes the directory; a file called "tests" is untouched.
    EXPECT_TRUE(excludedByOne("tests/", "tests/test_main.cpp"));
    EXPECT_FALSE(excludedByOne("tests/", "tests"));
}

TEST(FileFilterSelectionTest, AnAnchoredPatternMatchesOnlyFromTheRoot)
{
    EXPECT_TRUE(excludedByOne("docs/PLAN.md", "docs/PLAN.md"));
    EXPECT_FALSE(excludedByOne("docs/PLAN.md", "other/docs/PLAN.md"));
    // ...while a bare name matches at any depth.
    EXPECT_TRUE(excludedByOne("PLAN.md", "other/docs/PLAN.md"));
}

TEST(FileFilterSelectionTest, AnExtensionPatternRemovesEveryMatchAtAnyDepth)
{
    EXPECT_TRUE(excludedByOne("*.md", "README.md"));
    EXPECT_TRUE(excludedByOne("*.md", "docs/deep/NOTES.md"));
    EXPECT_FALSE(excludedByOne("*.md", "docs/NOTES.md.bak"));
}

TEST(FileFilterSelectionTest, TheLastMatchingRuleWins)
{
    FileFilter filter({ rule("tests"), includeRule("tests/keep.cpp") }, false);
    EXPECT_TRUE(filter.keeps("tests/keep.cpp")); // the later include re-admits it
    EXPECT_FALSE(filter.keeps("tests/drop.cpp"));
}

TEST(FileFilterSelectionTest, ARuleOnThePathBeatsARuleOnItsDirectory)
{
    // "*" removes everything by matching each file's own name; the deeper rule
    // (the file itself) wins over it.
    FileFilter filter({ rule("*"), includeRule("src/main.cpp") }, true);
    EXPECT_TRUE(filter.keeps("src/main.cpp"));
    EXPECT_FALSE(filter.keeps("src/util.cpp"));
    EXPECT_FALSE(filter.keeps("tests/test_main.cpp"));
}

TEST(FileFilterSelectionTest, IncludeModeKeepsOnlyWhatARuleNames)
{
    EXPECT_TRUE(includedByName("src", "src/main.cpp"));
    EXPECT_TRUE(includedByName("src", "src/ai/unit.cpp"));
    EXPECT_FALSE(includedByName("src", "tests/test_main.cpp"));
    EXPECT_FALSE(includedByName("src", "CMakeLists.txt"));
}

TEST(FileFilterSelectionTest, AnEmptyFilterExcludesNothing)
{
    const FileFilter filter;
    EXPECT_TRUE(filter.empty());
    EXPECT_FALSE(filter.whitelist());
    EXPECT_TRUE(filter.keeps("anything/at/all.cpp"));
}

TEST(FileFilterSelectionTest, TheDecidingRuleIsReportedAsWritten)
{
    const FileFilter filter({ rule("tests/", ".codebundlerignore") }, false);
    const FilterRule* decided = filter.decidingRule("tests/test_main.cpp");
    ASSERT_NE(decided, nullptr);
    EXPECT_EQ(decided->text, "tests/");
    EXPECT_EQ(decided->source, ".codebundlerignore");

    // Nothing matched: the mode decided, not a rule.
    EXPECT_EQ(filter.decidingRule("src/main.cpp"), nullptr);
}
