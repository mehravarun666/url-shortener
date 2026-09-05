#include "JwtFilter.h"
#include "JwtService.h"

#include <json/json.h>

#include <string_view>

void JwtFilter::doFilter(const drogon::HttpRequestPtr& req,
                         drogon::FilterCallback&& fcb,
                         drogon::FilterChainCallback&& fccb)
{
    const auto auth = req->getHeader("authorization");
    constexpr std::string_view kBearer = "Bearer ";
    if (auth.size() < kBearer.size() ||
        auth.compare(0, kBearer.size(), kBearer) != 0)
    {
        Json::Value body;
        body["error"]["code"] = "UNAUTHORIZED";
        body["error"]["message"] = "Missing or invalid Authorization header";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    const std::string token = auth.substr(kBearer.size());
    const auto claims = JwtService::verifyToken(token);
    if (!claims)
    {
        Json::Value body;
        body["error"]["code"] = "UNAUTHORIZED";
        body["error"]["message"] = "Invalid or expired token";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    req->attributes()->insert("user_id", claims->userId);
    req->attributes()->insert("email", claims->email);
    fccb();
}
