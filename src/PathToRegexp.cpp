/*
 * PathToRegexp.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: Dmitri Rubinstein
 */
#include "PathToRegexp.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include <iostream>

namespace HttpUtils
{

namespace
{

static std::regex PATH_REGEXP(
    // Match escaped characters that would otherwise appear in future matches.
    // This allows the user to escape special characters that won't transform.
    "(\\\\.)"
    // Or
    "|"
    // Match Express-style parameters and un-named parameters with a prefix
    // and optional suffixes. Matches appear as:
    //
    // "/:test(\\d+)?" => ["/", "test", "\d+", undefined, "?", undefined]
    // "/route(\\d+)"  => [undefined, undefined, undefined, "\d+", undefined, undefined]
    // "/*"            => ["/", undefined, undefined, undefined, undefined, "*"]
    "([\\/.])?(?:(?:\\:(\\w+)(?:\\(((?:\\\\.|[^()])+)\\))?|\\(((?:\\\\.|[^()])+)\\))([+*?])?|(\\*))");

/**
 * Escape a regular expression string.
 *
 * @param  {string} str
 * @return {string}
 */
static inline std::string escapeString(const std::string &str)
{
    std::string res;
    for (std::string::const_iterator it = str.begin(), et = str.end();
         it != et; ++it)
    {
        const char c = (*it);
        switch (c)
        {
            case '.': res+="\\."; continue;
            case '+': res+="\\+"; continue;
            case '*': res+="\\*"; continue;
            case '?': res+="\\?"; continue;
            case '=': res+="\\="; continue;
            case '^': res+="\\^"; continue;
            case '!': res+="\\!"; continue;
            case ':': res+="\\:"; continue;
            case '$': res+="\\$"; continue;
            case '{': res+="\\{"; continue;
            case '}': res+="\\}"; continue;
            case '(': res+="\\("; continue;
            case ')': res+="\\)"; continue;
            case '[': res+="\\["; continue;
            case ']': res+="\\]"; continue;
            case '|': res+="\\|"; continue;
            case '/': res+="\\/"; continue;
        }
        res += c;
    }
    return res;
}

/**
 * Escape the capturing group by escaping special characters and meaning.
 *
 * @param  {string} group
 * @return {string}
 */
static inline std::string escapeGroup(const std::string &group)
{
    std::string res;
    for (std::string::const_iterator it = group.begin(), et = group.end();
         it != et; ++it)
    {
        const char c = (*it);
        switch (c)
        {
            case '=': res+="\\="; continue;
            case '!': res+="\\!"; continue;
            case ':': res+="\\:"; continue;
            case '$': res+="\\$"; continue;
            case '/': res+="\\/"; continue;
            case '(':  res+="\\("; continue;
            case ')':  res+="\\)"; continue;
        }
        res += c;
    }
    return res;
}

static void hexchar(unsigned char c, unsigned char &hex1, unsigned char &hex2)
{
    hex1 = c / 16;
    hex2 = c % 16;
    hex1 += hex1 <= 9 ? '0' : 'A' - 10;
    hex2 += hex2 <= 9 ? '0' : 'A' - 10;
}

static std::string encodeURIComponent(const std::string & s)
{
    const char *str = s.c_str();
    std::string v;
    v.reserve(s.size());
    for (size_t i = 0, l = s.size(); i < l; i++)
    {
        const char c = s[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')')
        {
            v += c;
        }
        else if (c == ' ')
        {
            v += '+';
        }
        else
        {
            v += '%';
            unsigned char d1, d2;
            hexchar(c, d1, d2);
            v += d1;
            v += d2;
        }
    }

    return v;
}

static std::string to_string(const std::vector<std::string> &value)
{
    std::string s = "[";
    if (!value.empty())
    {
        auto it = value.begin();
        s += *it;
        auto end = value.end();
        while (it != end)
        {
            s += ", \"" + *it + "\"";
            ++it;
        }
    }
    s += "]";
    return s;
}

} // unnamed namespace

std::vector<PathToken> parsePath(const std::string &strArg)
{
    std::string str = strArg;
    std::vector<PathToken> tokens;
    int key = 0;
    int index = 0;
    std::string path;
    std::smatch res;
    std::string::const_iterator si = str.begin();
    const std::string::const_iterator send = str.end();

    while (std::regex_search(si, send, res, PATH_REGEXP))
    {
        std::string m = res[0];
        std::string escaped = res[1];
        path.append(str.substr(index, res.position()));
        index += res.position() + m.length();
        si = str.begin() + index;

        // Ignore already escaped sequences.
        if (!escaped.empty())
        {
            path.append(1, escaped[1]);
            continue;
        }

        // Push the current path onto the tokens.
        if (!path.empty())
        {
            tokens.push_back(std::move(path));
            path.clear();
        }

        std::string prefix = res[2];
        std::string name = res[3];
        std::string capture = res[4];
        std::string group = res[5];
        std::string suffix = res[6];
        std::string asterisk = res[7];

        bool repeat = suffix == "+" || suffix == "*";
        bool optional = suffix == "?" || suffix == "*";
        std::string delimiter = !prefix.empty() ? prefix : "/";
        std::string pattern =
            !capture.empty() ? capture :
                (!group.empty() ? group : (!asterisk.empty() ? ".*" : "[^" + delimiter + "]+?"));

        PathKey token;
        token.name = !name.empty() ? name : std::to_string(key++);
        token.prefix = prefix;
        token.delimiter = delimiter;
        token.optional = optional;
        token.repeat = repeat;
        token.pattern = escapeGroup(pattern); // FIXME
        tokens.push_back(token);
    }

    // Match any characters still remaining.
    if (index < str.length())
    {
        path.append(str.substr(index));
    }

    // If the path exists, push it onto the end.
    if (!path.empty())
    {
        tokens.push_back(std::move(path));
    }

    return tokens;
}

PathFunction::PathFunction(const std::vector<PathToken> &tokens)
    : tokens_(tokens)
    , matches_(tokens.size())
{
    init();
}

PathFunction::PathFunction(std::vector<PathToken> &&tokens)
    : tokens_(std::move(tokens))
    , matches_(tokens_.size())
{
    init();
}

PathFunction::PathFunction(const PathFunction &other)
    : tokens_(other.tokens_)
    , matches_()
{
    copyMatchesFrom(other);
}

PathFunction::PathFunction(PathFunction &&other)
    : tokens_(std::move(other.tokens_))
    , matches_(std::move(other.matches_))
{
}

PathFunction & PathFunction::operator=(const PathFunction &other)
{
    tokens_ = other.tokens_;
    copyMatchesFrom(other);
    return *this;
}

PathFunction & PathFunction::operator=(PathFunction &&other)
{
    tokens_ = std::move(other.tokens_);
    matches_ = std::move(other.matches_);
    return *this;
}

void PathFunction::init()
{
    // Compile all the tokens into regexps.
    // Compile all the patterns before compilation.
    typedef std::vector<PathToken>::size_type size_type;
    const size_type sz = tokens_.size();
    for (size_type i = 0; i < sz; ++i)
    {
        const PathToken &token = tokens_[i];
        if (token.which() == 1)
        {
            matches_[i].reset(new std::regex("^" + boost::get<PathKey>(token).pattern + "$"));
        }
    }
}

void PathFunction::copyMatchesFrom(const PathFunction &other)
{
    matches_.clear();
    matches_.reserve(other.matches_.size());
    for (auto it = other.matches_.begin(), et = other.matches_.end(); it != et; ++it)
    {
        std::unique_ptr<std::regex> el;
        if (*it)
            el.reset(new std::regex(*(*it)));
        matches_.push_back(std::move(el));
    }
}

std::string PathFunction::operator()(const std::map<std::string, std::vector<std::string> > &data)
{
    std::string path;
    typedef std::vector<PathToken>::size_type size_type;
    const size_type sz = tokens_.size();

    for (size_type i = 0; i < sz; ++i)
    {
        const PathToken &token = tokens_[i];

        if (token.which() == 0)
        {
            path += boost::get<std::string>(token);

            continue;
        }

        const PathKey &key = boost::get<PathKey>(token);

        auto it = data.find(key.name);
        if (it == data.end())
        {
            if (key.optional)
            {
                continue;
            }
            else
            {
                throw std::logic_error("Expected \"" + key.name + "\" to be defined");
            }
        }

        const std::vector<std::string> &value = it->second;

        if (!key.repeat && value.size() > 1)
        {
            throw std::logic_error("Expected \"" + key.name + "\" to not repeat, but received \"" + to_string(value) + "\"");
        }

        if (value.empty())
        {
            if (key.optional)
            {
                continue;
            }
            else
            {
                throw std::logic_error("Expected \"" + key.name + "\" to not be empty");
            }
        }

        std::string segment;
        const size_type value_sz = value.size();

        for (size_type j = 0; j < value_sz; j++)
        {
            segment = encodeURIComponent(value[j]);
            std::smatch res;

            if (!std::regex_search(segment, res, *matches_[i]))
            {
                throw std::logic_error(
                    "Expected all \"" + key.name + "\" to match \"" + key.pattern + "\", but received \"" + segment
                        + "\"");
            }

            path += (j == 0 ? key.prefix : key.delimiter) + segment;
        }
    }

    return path;
}


RegExp tokensToRegExp(const std::vector<PathToken> &tokens, int options)
{
    bool strict = (options & PR_STRICT) != 0;
    bool end = (options & PR_END) != 0;
    std::string route = "";
    PathToken lastToken = tokens.back();
    bool endsWithSlash = lastToken.which() == 0 && boost::algorithm::ends_with(boost::get<std::string>(lastToken), "/");

    // Iterate over the tokens and create our regexp string.
    for (auto it = tokens.begin(), eit = tokens.end(); it != eit; ++it)
    {
        const PathToken & token = *it;

        if (token.which() == 0)
        {
            route += escapeString(boost::get<std::string>(token));
        }
        else
        {
            const PathKey &key = boost::get<PathKey>(token);
            std::string prefix = escapeString(key.prefix);
            std::string capture = key.pattern;

            if (key.repeat)
            {
                capture += "(?:" + prefix + capture + ")*";
            }

            if (key.optional)
            {
                if (!prefix.empty())
                {
                    capture = "(?:" + prefix + "(" + capture + "))?";
                }
                else
                {
                    capture = "(" + capture + ")?";
                }
            }
            else
            {
                capture = prefix + "(" + capture + ")";
            }

            route += capture;
        }
    }

    // In non-strict mode we allow a slash at the end of match. If the path to
    // match already ends with a slash, we remove it for consistency. The slash
    // is valid at the end of a path match, not in the middle. This is important
    // in non-ending mode, where "/test/" shouldn't match "/test//route".
    if (!strict)
    {
        route = (endsWithSlash ? route.substr(0, route.length() - 2) : route) + "(?:\\/(?=$))?";
    }

    if (end)
    {
        route += '$';
    }
    else
    {
        // In non-ending mode, we need the capturing groups to match as much as
        // possible by using a positive lookahead to the end or next path segment.
        route += strict && endsWithSlash ? "" : "(?=\\/|$)";
    }

    return RegExp("^" + route, pathFlags(options));
}

RegExp pathToRegexp(const std::string &path, std::vector<PathKey> *keys, int options)
{
    std::vector<PathToken> tokens = parsePath(path);
    RegExp re = tokensToRegExp(tokens, options);

    // Attach keys back to the regexp.
    if (keys)
    {
        for (auto it = tokens.begin(), eit = tokens.end(); it != eit; ++it)
        {
            const PathToken & token = *it;
            if (token.which() != 0)
                keys->push_back(std::move(boost::get<PathKey>(token)));
        }
    }

    return re;
}

} // namespace PathToRegexp
