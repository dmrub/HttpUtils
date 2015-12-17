/**
 * C++11 port of https://github.com/pillarjs/path-to-regexp
 */
/*
 * PathToRegexp.hpp
 *
 *  Created on: Dec 8, 2015
 *      Author: Dmitri Rubinstein
 */

#ifndef PATHTOREGEXP_HPP_INCLUDED
#define PATHTOREGEXP_HPP_INCLUDED

#include <string>
#include <regex>
#include <utility>
#include <initializer_list>
#include <memory>
#include <map>
#include <boost/variant.hpp>

namespace HttpUtils
{

struct PathKey
{
    std::string name;
    std::string prefix;
    std::string delimiter;
    bool optional;
    bool repeat;
    std::string pattern;
};

typedef boost::variant<std::string, PathKey> PathToken;

typedef std::pair<std::string, std::regex::flag_type> RegExp;

enum PathOptions
{
    PR_SENSITIVE      = (1<<0),
    PR_STRICT         = (1<<1),
    PR_END            = (1<<2)
};

typedef std::map<std::string, std::vector<std::string> > SegmentMap;

class PathFunction
{
public:

    PathFunction(const std::vector<PathToken> &tokens);
    PathFunction(std::vector<PathToken> &&tokens);
    PathFunction(const PathFunction &other);
    PathFunction(PathFunction &&other);

    PathFunction & operator=(const PathFunction &other);
    PathFunction & operator=(PathFunction &&other);

    std::string operator()(const SegmentMap &data);

private:
    std::vector<PathToken> tokens_;
    std::vector< std::unique_ptr<std::regex> > matches_;

    void init();
    void copyMatchesFrom(const PathFunction &other);
};

inline std::regex::flag_type pathFlags(int options)
{
    return
        ((options & PR_SENSITIVE) != 0 ? std::regex::flag_type(0) : std::regex_constants::icase) |
        std::regex_constants::ECMAScript;
}

inline std::regex to_regex(const RegExp &re)
{
    return std::regex(re.first, re.second);
}

inline std::regex to_regex(RegExp &&re)
{
    return std::regex(std::move(re.first), std::move(re.second));
}

/**
 * Parse a string for the raw tokens.
 *
 * @param
 * @return list of tokens
 */
std::vector<PathToken> parsePath(const std::string &str);

/**
 * Expose a function for taking tokens and returning a RegExp.
 *
 * @param  tokens
 * @param  options
 * @return
 */
RegExp tokensToRegExp(const std::vector<PathToken> &tokens, int options = PR_END);

/**
 * Normalize the given path string, returning a regular expression.
 *
 * An empty array can be passed in for the keys, which will hold the
 * placeholder key descriptions. For example, using `/user/:id`, `keys` will
 * contain `[{ name: 'id', delimiter: '/', optional: false, repeat: false }]`.
 *
 * @param  path
 * @param  keys
 * @param  options
 * @return
 */
RegExp pathToRegexp(const std::string &path, std::vector<PathKey> *keys = 0, int options = PR_END);

inline RegExp pathToRegexp(const std::string &path, std::vector<PathKey> &keys, int options = PR_END)
{
    return pathToRegexp(path, &keys, options);
}

/**
 * Transform an iterable sequence into a regexp.
 *
 * @param  first
 * @param  last
 * @param  keys
 * @param  options
 * @return
 */
template < class It >
RegExp pathToRegexp(It first, It last, std::vector<PathKey> *keys = 0, int options = PR_END)
{
    std::string pattern = "(?:";

    if (first != last)
    {
        pattern += pathToRegexp(*first, keys, options).first;
        ++first;
    }

    while (first != last)
    {
        pattern += "|" + pathToRegexp(*first, keys, options).first;
        ++first;
    }

    pattern += ")";

    return RegExp(pattern, pathFlags(options));
}


inline RegExp pathToRegexp(std::initializer_list<const char *> paths, std::vector<PathKey> *keys = 0, int options = PR_END)
{
    return pathToRegexp(std::begin(paths), std::end(paths), keys, options);
}

inline RegExp pathToRegexp(std::initializer_list<const char *> paths, std::vector<PathKey> &keys, int options = PR_END)
{
    return pathToRegexp(std::begin(paths), std::end(paths), &keys, options);
}

/**
 * Compile a string to a template function for the path.
 *
 * @param  str
 * @return
 */
inline PathFunction compilePath(const std::string &str)
{
    return PathFunction(parsePath(str));
}

} // namespace HttpUtils

#endif /* PATHTOREGEXP_HPP_INCLUDED */
