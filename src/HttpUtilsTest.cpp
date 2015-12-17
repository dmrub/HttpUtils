/*
 * HttpUtilsTest.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: Dmitri Rubinstein
 */
#define CATCH_CONFIG_MAIN
#include "PathToRegexp.hpp"
#include "HttpRouter.hpp"
#include "catch.hpp"
#include <sstream>

using namespace HttpUtils;

namespace Catch
{
    template<> struct StringMaker<::HttpUtils::RegExp> {
        static std::string convert( ::HttpUtils::RegExp const& value ) {
            std::ostringstream oss;
            oss << "RegExp(\""<<value.first<<"\", "<<value.second<<")";
            return oss.str();
        }
    };
}

TEST_CASE("Convert tokens to regular expression", "[tokensToRegExp]") {

    REQUIRE(pathToRegexp("/:test/") ==
        RegExp("^\\/([^\\/]+?)(?:\\/(?=$))?$",
        std::regex_constants::icase|std::regex_constants::ECMAScript));

    REQUIRE(pathToRegexp("/:postType(video|audio|text)(\\+.+)?") ==
        RegExp("^\\/(video|audio|text)(\\+.+)?(?:\\/(?=$))?$",
        std::regex_constants::icase|std::regex_constants::ECMAScript));

    REQUIRE(pathToRegexp("/a/b/:postType(video|audio|text)(\\+.+)?") ==
        RegExp("^\\/a\\/b\\/(video|audio|text)(\\+.+)?(?:\\/(?=$))?$",
        std::regex_constants::icase|std::regex_constants::ECMAScript));

    REQUIRE(pathToRegexp("/a/b/:postType(video|audio|text)(\\+.+)?", 0, PR_SENSITIVE|PR_STRICT|PR_END) ==
        RegExp("^\\/a\\/b\\/(video|audio|text)(\\+.+)?$", std::regex_constants::ECMAScript));

    REQUIRE(pathToRegexp("/a/b/:postType(video|audio|text)(\\+.+)?", 0, PR_SENSITIVE|PR_STRICT) ==
        RegExp("^\\/a\\/b\\/(video|audio|text)(\\+.+)?(?=\\/|$)", std::regex_constants::ECMAScript));

    REQUIRE(pathToRegexp({"/:test(\\d+)?", "/route(\\d+)"}, 0, 0) ==
        RegExp("(?:^(?:\\/(\\d+))?(?:\\/(?=$))?(?=\\/|$)|^\\/route(\\d+)(?:\\/(?=$))?(?=\\/|$))",
        std::regex_constants::icase|std::regex_constants::ECMAScript));

    PathFunction pf = compilePath("/user/:id");
    SegmentMap sm;
    sm["id"] = {"123"};
    REQUIRE( pf(sm) == "/user/123" );
    sm["id"] = {"/"};
    REQUIRE( pf(sm) == "/user/%2F" );

    pf = compilePath("/:segment+");
    sm.clear();
    sm["segment"] = {"foo"};
    REQUIRE(pf(sm) == "/foo");
    sm["segment"] = {"a", "b", "c"};
    REQUIRE(pf(sm) == "/a/b/c");

    pf = compilePath("/user/:id(\\d+)");

    sm.clear();
    sm["id"] = {"123"};
    REQUIRE(pf(sm) == "/user/123");
    sm["id"] = {"abc"};
    REQUIRE_THROWS_AS(pf(sm), std::logic_error);
}


struct XRequest
{
    std::string method;
    std::string uriPath;

    XRequest()
        : method()
        , uriPath()
    { }

    XRequest(const std::string &method, const std::string uriPath)
        : method(method)
        , uriPath(uriPath)
    {

    }
};

struct XResponse
{
    std::vector<std::string> results;

    void clear()
    {
        results.clear();
    }
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

TEST_CASE("Test HttpRouter", "[httpRouter]") {
    XHttpRouter router;

    router.add("*", "/user/*", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::ostringstream oss;
        oss << "USER PROCESSING: " << req.method << " " << req.uriPath;
        res.results.push_back(oss.str());
        ctx.next();
    });
    router.add("GET", "/user/:id(\\d+)", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::ostringstream oss;
        oss << "USER AS INTEGER: " << ctx.match(1) << " " << req.method << " " << req.uriPath;
        res.results.push_back(oss.str());
    });
    router.add("GET", "/user/:str", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::ostringstream oss;
        oss << "USER AS STRING: " << ctx.match(1) << " " << req.method << " " << req.uriPath;
        res.results.push_back(oss.str());
    });
    router.add("PUT", "/data/:str", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::ostringstream oss;
        oss << ctx.match(1) << " " << req.method << " " << req.uriPath;
        res.results.push_back(oss.str());
    });
    router.add("*", "*", [=](XRequest &req, XResponse &res, XHttpRouter::Context &ctx) {
        std::ostringstream oss;
        oss << "DEFAULT: " << req.method << " " << req.uriPath;
        res.results.push_back(oss.str());
    });

    XRequest req;
    XResponse res;


    req = XRequest("GET", "/user/123");
    router.handleRequest(req, res);
    REQUIRE(res.results == std::vector<std::string>({"USER PROCESSING: GET /user/123", "USER AS INTEGER: 123 GET /user/123"}));
    res.clear();

    req = XRequest("GET", "/user/456");
    router.handleRequest(req, res);
    REQUIRE(res.results == std::vector<std::string>({"USER PROCESSING: GET /user/456", "USER AS INTEGER: 456 GET /user/456"}));
    res.clear();

    req = XRequest("GET", "/user/uid123");
    router.handleRequest(req, res);
    REQUIRE(res.results == std::vector<std::string>({"USER PROCESSING: GET /user/uid123", "USER AS STRING: uid123 GET /user/uid123"}));
    res.clear();

    req = XRequest("PUT", "/user/uid778");
    router.handleRequest(req, res);
    REQUIRE(res.results == std::vector<std::string>({"USER PROCESSING: PUT /user/uid778", "DEFAULT: PUT /user/uid778"}));
    res.clear();

    req = XRequest("PUT", "/data/foo");
    router.handleRequest(req, res);
    REQUIRE(res.results == std::vector<std::string>({"foo PUT /data/foo"}));
    res.clear();

    req = XRequest("PUT", "/data/bar");
    router.handleRequest(req, res);
    REQUIRE(res.results == std::vector<std::string>({"bar PUT /data/bar"}));
    res.clear();

    req = XRequest("PUT", "/user/789");
    router.handleRequest(req, res);
    REQUIRE(res.results == std::vector<std::string>({"USER PROCESSING: PUT /user/789", "DEFAULT: PUT /user/789"}));
    res.clear();
}
