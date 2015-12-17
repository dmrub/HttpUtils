/*
 * PathToRegexpTest.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: Dmitri Rubinstein
 */
#include "PathToRegexp.hpp"
#include "HttpRouter.hpp"
#include <iostream>

using namespace HttpUtils;

/** Creates C quoted string */
const std::string cquoted(const std::string &str)
{
    std::string res;
    res.reserve(str.size()+2);
    res+='"';
    for (std::string::const_iterator it = str.begin();
         it != str.end(); ++it)
    {
        const char c = (*it);

        switch (c)
        {
            case '\n': res+="\\n"; continue;
            case '\t': res+="\\t"; continue;
            case '\r': res+="\\r"; continue;
            case '\b': res+="\\b"; continue;
            case '\f': res+="\\f"; continue;
            case '\\': res+="\\\\"; continue;
            case '"':  res+="\\\""; continue;
        }

        if (::isgraph(c) || c == ' ')
        {
            res+=c; // printable and space
        }
        else
        {
            // use octal encoding
            char buf[10];
            ::sprintf(buf, "\\0%o", (int)c);
            res+=buf;
        }
    }
    res+='"';
    return res;
}

std::ostream & operator<<(std::ostream & out, const PathKey &key)
{
    out << "{ name: " << cquoted(key.name) << ",\n"
        << "    prefix: " << cquoted(key.prefix) << ",\n"
        << "    delimiter: " << cquoted(key.delimiter) << ",\n"
        << "    optional: " << (key.optional ? "true" : "false") << ",\n"
        << "    repeat: " << (key.repeat ? "true" : "false") << ",\n"
        << "    pattern: " << cquoted(key.pattern) << "}";
    return out;
}

class Printer : public boost::static_visitor<void>
{
public:
    void operator()(const std::string & str) const
    {
        std::cout << cquoted(str);
    }

    void operator()(const PathKey & key) const
    {
        std::cout << key;
    }
};

static std::regex PATH_REGEXP("(\\\\.)|([\\/.])?(?:(?:\\:(\\w+)(?:\\(((?:\\\\.|[^()])+)\\))?|\\(((?:\\\\.|[^()])+)\\))([+*?])?|(\\*))");

void testParse(const std::string &str)
{
    Printer p;

    std::string path;
    std::smatch res;
    std::string::size_type lastIndex = 0;
    std::string::const_iterator si = str.begin()+lastIndex;
    std::string::const_iterator send = str.end();

    if (std::regex_search(si, send, res, PATH_REGEXP))
    {
        std::cout << res[0] << std::endl;
    }
    else
    {
        std::cout << "NO MATCH" << std::endl;
    }

    std::vector<PathToken> tokens = parsePath(str);
    std::cout << "[" << std::endl;
    for (auto it = std::begin(tokens), et = std::end(tokens); it != et; ++it)
    {
        //std::cout << "  " << (*it) << std::endl;
        std::cout << "  ";

        boost::apply_visitor( p, *it );

        std::cout << ", " << std::endl;
    }
    std::cout << "]" << std::endl;
}

struct XRequest
{
    std::string method;
    std::string uriPath;

    XRequest(const std::string &method, const std::string uriPath)
        : method(method)
        , uriPath(uriPath)
    {

    }
};

struct XResponse
{

};

namespace HttpUtils
{

template <>
struct RequestTraits<XRequest>
{
    typedef XRequest & value_type;
    typedef value_type param_type;

    static std::string getMethod(param_type request)
    {
        return request.method;
    }

    static std::string getUriPath(param_type request)
    {
        return request.uriPath;
    }
};

} // namespace HttpUtils

typedef HttpUtils::HttpRouter<XRequest, XResponse> XHttpRouter;

int main(int argc, char **argv)
{
    //testParse("/:test(\\d+)?");
    //testParse("/route(\\d+)");
    //testParse("/*");
    //testParse("/:test/");
    //testParse("/:postType(video|audio|text)(\\+.+)?");
    //testParse("/a/b/:postType(video|audio|text)(\\+.+)?");

    //RegExp r1 = tokensToRegExp(parse("/:test/"));
    //RegExp r2 = tokensToRegExp(parse("/:postType(video|audio|text)(\\+.+)?"));
    //RegExp r3 = tokensToRegExp(parse("/a/b/:postType(video|audio|text)(\\+.+)?"), PR_SENSITIVE|PR_STRICT|PR_END);
    /*
    std::vector<Key> keys;
    RegExp r4 = pathToRegexp(
        {"/:test(\\d+)?", "/route(\\d+)"},
        keys,
        0);
    std::cout << cquoted(r4.first) << std::endl;
    for (auto it = keys.begin(), et = keys.end(); it != et; ++it)
    {
        std::cout << *it << std::endl;
    }
    */

    //std::vector<Token> tk = parse("/user/:id(\\d+)");
    //PathFunction pf(tk);
//    PathFunction pf = compile("/user/:id(\\d+)");
//    SegmentMap sm;
//    sm["id"] = {"123"};
//    std::string result = pf(sm);
//    std::cout << result << std::endl;

    XHttpRouter router;

    router.add("*", "/user/*", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::cout << "USER PROCESSING: " << req.method << " " << req.uriPath << std::endl;
        ctx.next();
    });
    router.add("GET", "/user/:id(\\d+)", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::cout << "USER AS INTEGER: " << ctx.match(1) << " " << req.method << " " << req.uriPath << std::endl;
    });
    router.add("GET", "/user/:str", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::cout << "USER AS STRING: " << ctx.match(1) << " " << req.method << " " << req.uriPath << std::endl;
    });
    router.add("PUT", "/data/:str", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::cout << ctx.match(1) << " " << req.method << " " << req.uriPath << std::endl;
    });
    router.add("*", "*", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::cout << "DEFAULT: " << req.method << " " << req.uriPath << std::endl;
    });

    XRequest req = XRequest("GET", "/user/123");
    XResponse res;
    router.handleRequest(req, res);

    req = XRequest("GET", "/user/456");
    router.handleRequest(req, res);

    req = XRequest("GET", "/user/uid123");
    router.handleRequest(req, res);

    req = XRequest("PUT", "/user/uid778");
    router.handleRequest(req, res);

    req = XRequest("PUT", "/data/foo");
    router.handleRequest(req, res);

    req = XRequest("PUT", "/data/bar");
    router.handleRequest(req, res);

    req = XRequest("PUT", "/user/789");
    router.handleRequest(req, res);
}
