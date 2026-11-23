#include "filefilter.hpp"
#include <utility>

namespace codebundler {

namespace {

    /**
     * @brief Matches a character class ('[' ... ']') against one character.
     * @param p Points at the '['.
     * @param c The character to test.
     * @param close Receives a pointer to the closing ']' on success.
     * @return 1 on a match, 0 on no match, -1 when this '[' does not open a
     *         class (no closing ']'), in which case the caller treats it as a
     *         literal '['.
     */
    int matchClass(const char* p, char c, const char** close)
    {
        const char* q = p + 1;
        bool negate = false;
        if (*q == '!' || *q == '^') {
            negate = true;
            ++q;
        }

        bool matched = false;
        bool first = true; // a ']' in the first position is a literal
        while (*q != '\0' && (*q != ']' || first)) {
            first = false;
            if (*q == '\\' && q[1] != '\0') {
                if (q[1] == c) {
                    matched = true;
                }
                q += 2;
                continue;
            }
            if (q[1] == '-' && q[2] != '\0' && q[2] != ']') {
                if (c >= q[0] && c <= q[2]) {
                    matched = true;
                }
                q += 3;
                continue;
            }
            if (*q == c) {
                matched = true;
            }
            ++q;
        }

        if (*q != ']') {
            return -1; // unterminated: not a class
        }
        *close = q;
        return (negate ? !matched : matched) ? 1 : 0;
    }

    /**
     * @brief The glob matcher behind FileFilter::globMatches.
     *
     * '*' and '?' stop at '/', "**" crosses it. Recursive, which is fine at the
     * sizes a path pattern has.
     */
    bool globHere(const char* p, const char* t)
    {
        while (*p != '\0') {
            if (*p == '\\') {
                if (p[1] == '\0') {
                    if (*t != '\\') {
                        return false;
                    }
                    ++t;
                    ++p;
                    continue;
                }
                if (*t != p[1]) {
                    return false;
                }
                p += 2;
                ++t;
                continue;
            }

            if (*p == '*') {
                if (p[1] == '*') {
                    const char* rest = p + 2;
                    if (*rest == '/') {
                        // "**/" stands for zero or more whole components, so it
                        // must align on a component boundary.
                        if (globHere(rest + 1, t)) {
                            return true;
                        }
                        for (const char* s = t; *s != '\0'; ++s) {
                            if (*s == '/' && globHere(rest + 1, s + 1)) {
                                return true;
                            }
                        }
                        return false;
                    }
                    for (const char* s = t;; ++s) {
                        if (globHere(rest, s)) {
                            return true;
                        }
                        if (*s == '\0') {
                            return false;
                        }
                    }
                }
                for (const char* s = t;; ++s) {
                    if (globHere(p + 1, s)) {
                        return true;
                    }
                    if (*s == '\0' || *s == '/') {
                        return false; // a single '*' never crosses a separator
                    }
                }
            }

            if (*p == '?') {
                if (*t == '\0' || *t == '/') {
                    return false;
                }
                ++p;
                ++t;
                continue;
            }

            if (*p == '[') {
                const char* close = nullptr;
                const int matched = matchClass(p, *t, &close);
                if (matched < 0) {
                    // Not a class after all: '[' is a literal.
                    if (*t != '[') {
                        return false;
                    }
                    ++p;
                    ++t;
                    continue;
                }
                if (*t == '\0' || *t == '/') {
                    return false;
                }
                if (matched == 0) {
                    return false;
                }
                p = close + 1;
                ++t;
                continue;
            }

            if (*p != *t) {
                return false;
            }
            ++p;
            ++t;
        }
        return *t == '\0';
    }

} // namespace

FileFilter::FileFilter(std::vector<FilterRule> rules, bool whitelist)
    : m_rules(std::move(rules))
    , m_whitelist(whitelist)
{
}

bool FileFilter::globMatches(const std::string& pattern, const std::string& text)
{
    return globHere(pattern.c_str(), text.c_str());
}

bool FileFilter::anyComponentMatches(const std::string& pattern, const std::string& path)
{
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::string component
            = (slash == std::string::npos) ? path.substr(start) : path.substr(start, slash - start);
        if (!component.empty() && globMatches(pattern, component)) {
            return true;
        }
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return false;
}

bool FileFilter::parseRule(const std::string& line, const std::string& source, FilterRule& out)
{
    std::string text = line;
    if (!text.empty() && text.back() == '\r') {
        text.pop_back(); // a file written on another platform
    }
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return false; // blank
    }
    const std::size_t last = text.find_last_not_of(" \t");
    text = text.substr(first, last - first + 1);
    if (text.empty() || text[0] == '#') {
        return false; // comment
    }

    out = FilterRule {};
    out.text = text;
    out.source = source;

    if (text[0] == '!') {
        out.include = true;
        text.erase(0, 1);
    }
    if (!text.empty() && text.back() == '/') {
        out.directoryOnly = true;
        text.pop_back();
    }
    if (!text.empty() && text[0] == '/') {
        text.erase(0, 1);
        out.rootAnchored = true;
    } else {
        out.rootAnchored = text.find('/') != std::string::npos;
    }
    if (text.empty()) {
        return false; // a rule that names nothing
    }

    out.pattern = text;
    return true;
}

bool FileFilter::ruleMatches(const FilterRule& rule, const std::string& candidate, bool candidateIsDirectory)
{
    if (rule.pattern.empty()) {
        return false;
    }
    if (rule.directoryOnly && !candidateIsDirectory) {
        return false;
    }
    if (rule.rootAnchored) {
        return globMatches(rule.pattern, candidate);
    }
    return anyComponentMatches(rule.pattern, candidate);
}

const FilterRule* FileFilter::decidingRule(const std::string& relativePath) const
{
    if (relativePath.empty()) {
        return nullptr;
    }

    // The path itself first, then each of its directories: a rule matching a
    // directory governs the subtree, a rule matching the path overrides it.
    std::string candidate = relativePath;
    bool isDirectory = false; // the first candidate is the path itself
    for (;;) {
        for (auto rule = m_rules.rbegin(); rule != m_rules.rend(); ++rule) {
            if (ruleMatches(*rule, candidate, isDirectory)) {
                return &*rule;
            }
        }
        const std::size_t slash = candidate.rfind('/');
        if (slash == std::string::npos) {
            break;
        }
        candidate.erase(slash); // now one of the path's directories
        isDirectory = true;
    }
    return nullptr;
}

bool FileFilter::keeps(const std::string& relativePath) const
{
    const FilterRule* rule = decidingRule(relativePath);
    if (rule != nullptr) {
        return rule->include;
    }
    return !m_whitelist;
}

} // namespace codebundler
