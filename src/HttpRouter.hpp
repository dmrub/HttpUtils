/*
 * HttpRouter.hpp
 *
 *  Created on: Dec 11, 2015
 *      Author: Dmitri Rubinstein
 */

#ifndef HTTPROUTER_HPP_INCLUDED
#define HTTPROUTER_HPP_INCLUDED

#include <functional>
#include <stdexcept>
#include "PathToRegexp.hpp"

namespace HttpUtils
{

template <class Request>
struct RequestTraits
{
    typedef Request & value_type;
    typedef value_type param_type;
};

template <class Response>
struct ResponseTraits
{
    typedef Response & value_type;
    typedef value_type param_type;
};

template <class Request, class Response>
class HttpRouter
{
public:
    typedef Request RequestType;
    typedef Response ResponseType;
    typedef typename RequestTraits<Request>::param_type RequestParamType;
    typedef typename ResponseTraits<Response>::param_type ResponseParamType;
    typedef typename RequestTraits<Request>::value_type RequestValueType;
    typedef typename ResponseTraits<Response>::value_type ResponseValueType;

    class Context;
    typedef std::function<void(RequestParamType, ResponseParamType, Context &)> Handler;
private:
    struct Matcher
    {
        std::string method;
        std::regex pathRegex;
        Handler handler;

        Matcher(const std::string &method, const std::regex &pathRegex, Handler handler)
            : method(method), pathRegex(pathRegex), handler(handler)
        {
        }

        Matcher(const std::string &method, std::regex &&pathRegex, Handler &&handler)
            : method(method), pathRegex(std::move(pathRegex)), handler(std::move(handler))
        {
        }

    };
    typedef std::vector<Matcher> MatcherList;
public:

    HttpRouter() : matchers_() { }
    HttpRouter(const HttpRouter &other) : matchers_(other) { }
    HttpRouter(HttpRouter &&other) : matchers_(std::move(other)) { }

    HttpRouter & operator=(const HttpRouter &other)
    {
        if (this != &other)
        {
            matchers_ = other.matchers_;
        }
        return *this;
    }

    HttpRouter & operator=(HttpRouter &&other)
    {
        if (this != &other)
        {
            matchers_ = std::move(other.matchers_);
        }
    }

    class Context
    {
        friend class HttpRouter;
    public:

        void next()
        {

            for (; current_ != end_; ++current_)
            {
                if (method_ == current_->method ||
                    current_->method.empty() ||
                    current_->method == "*")
                {
                    if (std::regex_search(uriPath_, match_, current_->pathRegex))
                    {
                        const Matcher &matcher = *current_++;
                        matcher.handler(request_, response_, *this);
                        return;
                    }
                }
            }
        }

        std::smatch::string_type match(std::smatch::size_type i = 0) const
        {
            return match_.str(i);
        }

    private:

        Context(RequestParamType request, ResponseParamType response, const MatcherList &matchers)
            : request_(request)
            , response_(response)
            , method_(RequestTraits<Request>::getMethod(request))
            , uriPath_(RequestTraits<Request>::getUriPath(request))
            , current_(std::begin(matchers))
            , end_(std::end(matchers))
        {
        }

        void handle()
        {
            next();
        }

        RequestValueType request_;
        ResponseValueType response_;
        std::string method_;
        std::string uriPath_;
        typename MatcherList::const_iterator current_;
        typename MatcherList::const_iterator end_;
        std::smatch match_;
    };

    void add(const std::string &method, const std::string &path, Handler handler)
    {
        std::vector<PathKey> keys;
        std::regex re = to_regex(pathToRegexp(path, keys));
        matchers_.emplace_back(method, std::move(re), std::move(handler));
    }

    void handleRequest(RequestParamType request, ResponseParamType response) const
    {
        Context ctx(request, response, matchers_);
        ctx.handle();
    }

private:
    MatcherList matchers_;
};

} // namespace HttpUtils

#endif /* HTTPROUTER_HPP_INCLUDED */
