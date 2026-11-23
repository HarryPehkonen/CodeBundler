#ifndef CODEBUNDLER_FILEFILTER_HPP
#define CODEBUNDLER_FILEFILTER_HPP

#include <string>
#include <vector>

namespace codebundler {

/**
 * @brief One selection rule: a glob pattern plus the direction it selects.
 *
 * A rule is written as a bare pattern (selects the match for exclusion) or with
 * a leading '!' (selects it for inclusion: a re-include in exclude mode, and
 * the only way into the bundle in include mode). A trailing '/' limits the rule
 * to directories, so "tests/" prunes a directory without also matching a file
 * that happens to be called "tests".
 */
struct FilterRule {
    std::string pattern; ///< the glob, without the leading '!' or the trailing '/'
    bool include = false; ///< '!' present: this rule admits rather than removes
    bool directoryOnly = false; ///< trailing '/': matches directories, never a file
    bool rootAnchored = false; ///< the pattern names a path: match it from the repository root
    std::string text; ///< the rule as written, for the bundle's note
    std::string source; ///< where it came from: ".codebundlerignore", "--exclude", "--include"
};

/**
 * @brief Decides which tracked files a bundle carries.
 *
 * Rules are evaluated in order and the LAST rule that matches a path decides
 * it, so a later rule overrides an earlier one (a CLI include can re-admit
 * something the ignore file removed). A path is judged against itself and
 * against each of its parent directories, deepest first: a rule that matches a
 * directory governs its whole subtree, and a rule matching the path itself
 * overrides it.
 *
 * Pattern syntax (a gitignore-flavoured glob, not a shell expansion):
 *
 *   name        matches that path component at any depth ("tests" prunes every
 *               tests/ directory, "*.md" removes every .md file)
 *   a/b         contains a slash: matched against the whole path from the
 *               repository root, so "docs/PLAN.md" is that one file
 *   *  ?        match within one path component ('*' and '?' never cross '/')
 *   **          crosses '/', so "docs/**" is every file under docs/
 *   [a-z]       a character class; '[!a]' or '[^a]' negates it
 *   \x          escapes x
 *   !pattern    include instead of exclude
 *   pattern/    directories only
 *   /pattern    anchored at the repository root even without another '/'
 */
class FileFilter {
public:
    FileFilter() = default;

    /**
     * @param rules Rules in evaluation order; the last one that matches wins.
     * @param whitelist True when the caller named something to include, so a
     *        path that no rule admits is left out rather than bundled.
     */
    FileFilter(std::vector<FilterRule> rules, bool whitelist);

    /** @brief True when a path nothing admits is left out (include mode). */
    bool whitelist() const { return m_whitelist; }

    bool empty() const { return m_rules.empty(); }

    const std::vector<FilterRule>& rules() const { return m_rules; }

    /**
     * @brief Whether this path belongs in the bundle.
     * @param relativePath The path as `git ls-files` reported it.
     */
    bool keeps(const std::string& relativePath) const;

    /**
     * @brief The rule that decided this path, or nullptr when nothing matched
     * and the mode's default decided it.
     */
    const FilterRule* decidingRule(const std::string& relativePath) const;

    /**
     * @brief Parses one rule line or CLI argument.
     * @param line The text: a '.codebundlerignore' line or a CLI pattern.
     * @param source Where it came from, recorded for the note.
     * @param out Receives the rule when the text was one.
     * @return False for a blank line, a '#' comment or text that names nothing;
     *         nothing is added to the filter in that case.
     */
    static bool parseRule(const std::string& line, const std::string& source, FilterRule& out);

    /**
     * @brief Whether a rule matches one candidate path (the file, or one of its
     * directories).
     * @param candidateIsDirectory True when the candidate is a parent directory.
     */
    static bool ruleMatches(const FilterRule& rule, const std::string& candidate, bool candidateIsDirectory);

    /**
     * @brief Glob match: '*' and '?' stop at '/', '**' crosses it, '[...]' is a
     * class, '\' escapes the next character.
     */
    static bool globMatches(const std::string& pattern, const std::string& text);

    /** @brief Whether any single path component matches a slash-free glob. */
    static bool anyComponentMatches(const std::string& pattern, const std::string& path);

private:
    std::vector<FilterRule> m_rules;
    bool m_whitelist = false;
};

} // namespace codebundler

#endif // CODEBUNDLER_FILEFILTER_HPP
